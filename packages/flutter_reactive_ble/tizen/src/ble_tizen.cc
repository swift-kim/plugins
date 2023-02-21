// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ble_tizen.h"

#include <bluetooth.h>
#include <bluetooth_type.h>

#include "log.h"

namespace {

Uuid GetUuid(const bt_gatt_h &gatt_handle) {
  char *uuid_cstr = nullptr;
  int ret = bt_gatt_get_uuid(gatt_handle, &uuid_cstr);
  if (ret != BT_ERROR_NONE) {
    return std::string();
  }
  std::string uuid = std::string(uuid_cstr);
  free(uuid_cstr);
  return uuid;
}

void AddDiscoveredService(bt_gatt_h service_handle,
                          std::vector<DiscoveredService> *discovered_services) {
  DiscoveredService discovered_service;
  discovered_service.service_id = GetUuid(service_handle);

  bt_gatt_service_foreach_included_services(
      service_handle,
      [](int total, int index, bt_gatt_h included_service_handle,
         void *user_data) -> bool {
        auto *discovered_service = static_cast<DiscoveredService *>(user_data);
        AddDiscoveredService(included_service_handle,
                             &discovered_service->included_services);
        return true;
      },
      &discovered_service);
  bt_gatt_service_foreach_characteristics(
      service_handle,
      [](int total, int index, bt_gatt_h characteristic_handle,
         void *user_data) -> bool {
        int properties = 0;
        bt_gatt_characteristic_get_properties(characteristic_handle,
                                              &properties);

        DiscoveredCharacteristic characteristic = {};
        characteristic.characteristic_id = GetUuid(characteristic_handle);
        characteristic.is_readable = properties & BT_GATT_PROPERTY_READ;
        characteristic.is_writable_with_response =
            properties & BT_GATT_PROPERTY_WRITE;
        characteristic.is_writable_without_response =
            properties & BT_GATT_PROPERTY_WRITE_WITHOUT_RESPONSE;
        characteristic.is_notifiable = properties & BT_GATT_PROPERTY_NOTIFY;
        characteristic.is_indicatable = properties & BT_GATT_PROPERTY_INDICATE;

        auto *discovered_service = static_cast<DiscoveredService *>(user_data);
        characteristic.service_id = discovered_service->service_id;
        discovered_service->characteristic_ids.push_back(
            characteristic.characteristic_id);
        discovered_service->characteristics.push_back(characteristic);
        return true;
      },
      &discovered_service);

  discovered_services->push_back(discovered_service);
}

}  // namespace

bool BleTizen::Initialize() {
  last_error_ = bt_initialize();
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  return true;
}

bool BleTizen::Deinitialize() {
  last_error_ = bt_deinitialize();
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  return true;
}

std::optional<BleStatus> BleTizen::GetBleStatus() {
  bt_adapter_state_e adapter_state;
  last_error_ = bt_adapter_get_state(&adapter_state);
  if (last_error_ == BT_ERROR_NONE) {
    return adapter_state == BT_ADAPTER_ENABLED ? BleStatus::kEnabled
                                               : BleStatus::kDisabled;
  } else if (last_error_ == BT_ERROR_NOT_SUPPORTED) {
    return BleStatus::kNotSupported;
  }
  return std::nullopt;
}

bool BleTizen::SetBleStatusChangedCallback(BleStatusChangedCallback callback) {
  last_error_ = bt_adapter_set_state_changed_cb(
      [](int result, bt_adapter_state_e adapter_state, void *user_data) {
        BleTizen *self = static_cast<BleTizen *>(user_data);

        if (self->ble_status_changed_callback_) {
          self->ble_status_changed_callback_(adapter_state == BT_ADAPTER_ENABLED
                                                 ? BleStatus::kEnabled
                                                 : BleStatus::kDisabled);
        }
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  ble_status_changed_callback_ = std::move(callback);
  return true;
}

bool BleTizen::UnsetBleStatusChangedCallback() {
  last_error_ = bt_adapter_unset_state_changed_cb();
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  ble_status_changed_callback_ = nullptr;
  return true;
}

bool BleTizen::Scan(const std::vector<Uuid> &service_ids) {
  if (is_scanning_) {
    StopScan();
  }

  for (const auto &uuid : service_ids) {
    std::unique_ptr<ScanFilter> filter = std::make_unique<ScanFilter>(uuid);
    if (filter->Register()) {
      filters_.emplace_back(std::move(filter));
    } else {
      LOG_ERROR("Ignoring scan filter: %s", uuid.c_str());
    }
  }

  // TODO: Handle locks when callbacks are run by different threads.
  last_error_ = bt_adapter_le_start_scan(
      [](int result, bt_adapter_le_device_scan_result_info_s *info,
         void *user_data) {
        BleTizen *self = static_cast<BleTizen *>(user_data);

        std::unique_ptr<DeviceScanInfo> scan_info =
            std::make_unique<DeviceScanInfo>();
        scan_info->device_id = std::string(info->remote_address);
        scan_info->rssi = info->rssi;

        char *name = nullptr;
        int ret = bt_adapter_le_get_scan_result_device_name(
            info, BT_ADAPTER_LE_PACKET_ADVERTISING, &name);
        if (ret == BT_ERROR_NONE) {
          scan_info->name = std::string(name);
          free(name);
        } else if (ret == BT_ERROR_NO_DATA) {
          // TODO: Retry with the BT_ADAPTER_LE_PACKET_SCAN_RESPONSE type.
          LOG_INFO("Could not obtain name info from the device %s.",
                   info->remote_address);
        } else {
          self->last_error_ = ret;
          return;
        }

        int manufacturer_id;
        char *manufacturer_data = nullptr;
        int manufacturer_data_len = 0;
        ret = bt_adapter_le_get_scan_result_manufacturer_data(
            info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &manufacturer_id,
            &manufacturer_data, &manufacturer_data_len);
        if (ret == BT_ERROR_NONE) {
          scan_info->manufacturer_data = std::vector<uint8_t>(
              manufacturer_data, manufacturer_data + manufacturer_data_len);
          free(manufacturer_data);
        } else if (ret == BT_ERROR_NO_DATA) {
          LOG_INFO("Could not obtain manufacturer info from the device %s.",
                   info->remote_address);
        } else {
          self->last_error_ = ret;
          return;
        }

        char **service_ids = nullptr;
        int service_count = 0;
        ret = bt_adapter_le_get_scan_result_service_uuids(
            info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &service_ids,
            &service_count);
        if (ret == BT_ERROR_NONE) {
          for (int i = 0; i < service_count; ++i) {
            scan_info->service_ids.emplace_back(std::string(service_ids[i]));
            free(service_ids[i]);
          }
          free(service_ids);
        } else if (ret == BT_ERROR_NO_DATA) {
          LOG_INFO("Could not obtain service UUIDs from the device %s.",
                   info->remote_address);
        } else {
          self->last_error_ = ret;
          return;
        }

        bt_adapter_le_service_data_s *service_data = nullptr;
        int service_data_count = 0;
        ret = bt_adapter_le_get_scan_result_service_data_list(
            info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &service_data,
            &service_data_count);
        if (ret == BT_ERROR_NONE) {
          for (int i = 0; i < service_data_count; ++i) {
            ServiceDataEntry entry = {};
            entry.service_id = service_data[i].service_uuid;
            entry.data =
                std::vector<uint8_t>(service_data[i].service_data,
                                     service_data[i].service_data +
                                         service_data[i].service_data_len);
            scan_info->service_data.emplace_back(entry);
          }
          bt_adapter_le_free_service_data_list(service_data,
                                               service_data_count);

        } else if (ret == BT_ERROR_NO_DATA) {
          LOG_INFO("Could not obtain the service data from the device %s.",
                   info->remote_address);
        } else {
          self->last_error_ = ret;
          return;
        }

        if (self->device_scan_callback_) {
          self->device_scan_callback_(std::move(scan_info));
        }
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }

  is_scanning_ = true;
  return true;
}

bool BleTizen::StopScan() {
  if (!is_scanning_) {
    return true;
  }
  last_error_ = bt_adapter_le_stop_scan();
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  ScanFilter::UnregisterAll();
  filters_.clear();

  is_scanning_ = false;
  return true;
}

bool BleTizen::ConnectToDevice(const std::string &device_id) {
  last_error_ = bt_gatt_connect(device_id.c_str(), false);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  if (connection_state_changed_callback_) {
    connection_state_changed_callback_(device_id, ConnectionState::kConnecting);
  }
  return true;
}

bool BleTizen::DisconnectFromDevice(const std::string &device_id) {
  last_error_ = bt_gatt_disconnect(device_id.c_str());
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  if (connection_state_changed_callback_) {
    connection_state_changed_callback_(device_id,
                                       ConnectionState::kDisconnecting);
  }
  return true;
}

bool BleTizen::SetConnectionStateChangedCallback(
    ConnectionStateChangedCallback callback) {
  last_error_ = bt_gatt_set_connection_state_changed_cb(
      [](int result, bool connected, const char *remote_address,
         void *user_data) {
        BleTizen *self = static_cast<BleTizen *>(user_data);
        if (self->connection_state_changed_callback_) {
          self->connection_state_changed_callback_(
              remote_address, connected ? ConnectionState::kConnected
                                        : ConnectionState::kDisconnected);
        }
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  connection_state_changed_callback_ = callback;
  return true;
}

bool BleTizen::UnsetConnectionStateChangedCallback() {
  last_error_ = bt_gatt_unset_connection_state_changed_cb();
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  connection_state_changed_callback_ = nullptr;
  return true;
}

bool BleTizen::ReadCharacteristics(
    const QualifiedCharacteristic &characteristic) {
  auto [it, inserted] =
      native_handles_.try_emplace(characteristic, characteristic);
  if (inserted) {
    LOG_DEBUG("A new characteristic info was inserted: [%s %s %s].",
              characteristic.device_id().c_str(),
              characteristic.service_id().c_str(),
              characteristic.characteristic_id().c_str());
  } else {
    LOG_DEBUG("Using exising characteristic info: [%s %s %s].",
              characteristic.device_id().c_str(),
              characteristic.service_id().c_str(),
              characteristic.characteristic_id().c_str());
  }
  CharacteristicHandle *native_handle = &it->second;
  if (!native_handle->IsReadable()) {
    LOG_ERROR("The characteristic is not readable.");
    last_error_ = BT_ERROR_NOT_SUPPORTED;
    return false;
  }
  native_handle->SetValueReadCallback(
      [this](QualifiedCharacteristic characteristic,
             const std::vector<uint8_t> &data) {
        if (notification_callback_) {
          notification_callback_(characteristic, data);
        }
      });

  // TODO: Handle locks when callbacks are run by different threads.
  last_error_ = bt_gatt_client_read_value(
      native_handle->characteristic_handle(),
      [](int result, bt_gatt_h request_handle, void *user_data) {
        if (result != BT_ERROR_NONE) {
          // TODO: Add error callbacks.
          LOG_ERROR("Read value returns error on callback: %s",
                    get_error_message(result));
          return;
        }
        CharacteristicHandle *native_handle =
            static_cast<CharacteristicHandle *>(user_data);
        char *value;
        int length;
        bt_gatt_get_value(request_handle, &value, &length);
        std::vector<uint8_t> bytes(value, value + length);
        free(value);
        native_handle->NotifyValueRead(bytes);
        native_handle->SetValueReadCallback(nullptr);
      },
      native_handle);
  if (last_error_ != BT_ERROR_NONE) {
    native_handle->SetValueReadCallback(nullptr);
    return false;
  }
  return true;
}

bool BleTizen::WriteCharacteristics(
    const QualifiedCharacteristic &characteristic,
    const std::vector<uint8_t> &value,
    CharacteristicValueWriteCallback callback) {
  auto [it, _] = native_handles_.try_emplace(characteristic, characteristic);
  CharacteristicHandle *native_handle = &it->second;
  last_error_ = bt_gatt_set_value(native_handle->characteristic_handle(),
                                  reinterpret_cast<const char *>(value.data()),
                                  static_cast<int>(value.size()));
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }

  native_handle->SetValueWroteCallback(callback);
  // TODO: Handle locks when callbacks are run by different threads.
  last_error_ = bt_gatt_client_write_value(
      native_handle->characteristic_handle(),
      [](int result, bt_gatt_h request_handle, void *user_data) {
        if (result != BT_ERROR_NONE) {
          // TODO: Add error callbacks.
        }
        CharacteristicHandle *native_handle =
            static_cast<CharacteristicHandle *>(user_data);
        native_handle->NotifyValueWrote();
        native_handle->SetValueWroteCallback(nullptr);
      },
      native_handle);
  if (last_error_ != BT_ERROR_NONE) {
    native_handle->SetValueWroteCallback(nullptr);
    return false;
  }
  return true;
}

bool BleTizen::ListenNotifications(
    const QualifiedCharacteristic &characteristic) {
  auto [it, _] = native_handles_.try_emplace(characteristic, characteristic);
  CharacteristicHandle *native_handle = &it->second;
  native_handle->SetValueChangedCallback(
      [this](QualifiedCharacteristic characteristic,
             const std::vector<uint8_t> &data) {
        if (notification_callback_) {
          notification_callback_(characteristic, data);
        }
      });

  // TODO: Handle locks when callbacks are run by different threads.
  last_error_ = bt_gatt_client_set_characteristic_value_changed_cb(
      native_handle->characteristic_handle(),
      [](bt_gatt_h characteristic, char *value, int len, void *user_data) {
        CharacteristicHandle *native_handle =
            static_cast<CharacteristicHandle *>(user_data);
        std::vector<uint8_t> bytes(value, value + len);
        native_handle->NotifyValueChanged(bytes);
      },
      native_handle);
  if (last_error_ != BT_ERROR_NONE) {
    native_handle->SetValueChangedCallback(nullptr);
    return false;
  }
  return true;
}

bool BleTizen::StopNotifications(
    const QualifiedCharacteristic &characteristic) {
  auto it = native_handles_.find(characteristic);
  if (it == native_handles_.end()) {
    return true;
  }
  CharacteristicHandle *native_handle = &it->second;
  last_error_ = bt_gatt_client_unset_characteristic_value_changed_cb(
      native_handle->characteristic_handle());
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  native_handle->SetValueChangedCallback(nullptr);
  return true;
}

std::optional<std::vector<DiscoveredService>> BleTizen::DiscoverServices(
    const std::string &device_id) {
  std::vector<DiscoveredService> discovered_services;

  bt_gatt_client_h device_handle;
  last_error_ = bt_gatt_client_create(device_id.c_str(), &device_handle);
  if (last_error_ != BT_ERROR_NONE) {
    return std::nullopt;
  }
  last_error_ = bt_gatt_client_foreach_services(
      device_handle,
      [](int total, int index, bt_gatt_h service_handle, void *user_data) {
        std::vector<DiscoveredService> *discovered_services =
            static_cast<std::vector<DiscoveredService> *>(user_data);
        AddDiscoveredService(service_handle, discovered_services);
        return true;
      },
      &discovered_services);
  if (last_error_ != BT_ERROR_NONE) {
    return std::nullopt;
  }

  last_error_ = bt_gatt_client_destroy(device_handle);
  if (last_error_ != BT_ERROR_NONE) {
    return std::nullopt;
  }
  return discovered_services;
}

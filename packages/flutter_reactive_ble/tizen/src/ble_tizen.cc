// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ble_tizen.h"

#include <bluetooth.h>

#include "log.h"

namespace {

Uuid GetUuid(const bt_gatt_h &gatt_handle) {
  char *uuid = nullptr;
  int ret = bt_gatt_get_uuid(gatt_handle, &uuid);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to retrieve a UUID: %s", get_error_message(ret));
    return std::string();
  }
  std::string result = std::string(uuid);
  free(uuid);
  return result;
}

void AddDiscoveredService(bt_gatt_h service_handle,
                          std::vector<DiscoveredService> *discovered_services) {
  DiscoveredService discovered_service = {};
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
        int ret = bt_gatt_characteristic_get_properties(characteristic_handle,
                                                        &properties);
        if (ret != BT_ERROR_NONE) {
          LOG_ERROR("Unable to get characteristic properties: %s",
                    get_error_message(ret));
          return true;
        }

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
        discovered_service->characteristic_ids.emplace_back(
            characteristic.characteristic_id);
        discovered_service->characteristics.emplace_back(characteristic);
        return true;
      },
      &discovered_service);

  discovered_services->emplace_back(discovered_service);
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

BleStatus BleTizen::GetBleStatus() {
  bt_adapter_state_e adapter_state;
  last_error_ = bt_adapter_get_state(&adapter_state);
  if (last_error_ == BT_ERROR_NONE) {
    return adapter_state == BT_ADAPTER_ENABLED ? BleStatus::kEnabled
                                               : BleStatus::kDisabled;
  } else if (last_error_ == BT_ERROR_NOT_SUPPORTED) {
    return BleStatus::kNotSupported;
  }
  return BleStatus::kUnknown;
}

bool BleTizen::SetBleStatusChangeCallback(BleStatusChangeCallback callback) {
  last_error_ = bt_adapter_set_state_changed_cb(
      [](int result, bt_adapter_state_e adapter_state, void *user_data) {
        if (result != BT_ERROR_NONE) {
          LOG_ERROR("The operation failed unexpectedly: %s",
                    get_error_message(result));
          return;
        }
        BleTizen *self = static_cast<BleTizen *>(user_data);
        if (self->ble_status_change_callback_) {
          BleStatus status = adapter_state == BT_ADAPTER_ENABLED
                                 ? BleStatus::kEnabled
                                 : BleStatus::kDisabled;
          self->ble_status_change_callback_(status);
        }
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  ble_status_change_callback_ = std::move(callback);
  return true;
}

bool BleTizen::UnsetBleStatusChangeCallback() {
  last_error_ = bt_adapter_unset_state_changed_cb();
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  ble_status_change_callback_ = nullptr;
  return true;
}

bool BleTizen::Scan(const std::vector<Uuid> &service_ids) {
  if (is_scanning_) {
    StopScan();
  }

  for (const Uuid &service_id : service_ids) {
    std::unique_ptr<ScanFilter> filter =
        std::make_unique<ScanFilter>(service_id);
    if (filter->Register()) {
      filters_.emplace_back(std::move(filter));
    } else {
      LOG_ERROR("Ignoring scan filter: %s", service_id.c_str());
    }
  }

  last_error_ = bt_adapter_le_start_scan(
      [](int result, bt_adapter_le_device_scan_result_info_s *info,
         void *user_data) {
        BleTizen *self = static_cast<BleTizen *>(user_data);
        if (result != BT_ERROR_NONE) {
          LOG_ERROR("The scan operation resulted in an error: %s",
                    get_error_message(result));
          if (self->device_scan_error_callback_) {
            self->device_scan_error_callback_(result,
                                              get_error_message(result));
          }
          return;
        }

        DiscoveredDevice device = {};
        device.device_id = std::string(info->remote_address);
        device.rssi = info->rssi;

        char *name = nullptr;
        int ret = bt_adapter_le_get_scan_result_device_name(
            info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &name);
        if (ret == BT_ERROR_NO_DATA) {
          // Retry with a different packet type.
          ret = bt_adapter_le_get_scan_result_device_name(
              info, BT_ADAPTER_LE_PACKET_ADVERTISING, &name);
        }
        if (ret == BT_ERROR_NONE) {
          device.name = std::string(name);
          free(name);
        } else if (ret != BT_ERROR_NO_DATA) {
          // TODO(swift-kim): Add error callbacks.
          LOG_ERROR("Could not obtain name info from the device %s.",
                    info->remote_address);
          return;
        }

        int manufacturer_id = 0;
        char *manufacturer_data = nullptr;
        int manufacturer_data_len = 0;
        ret = bt_adapter_le_get_scan_result_manufacturer_data(
            info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &manufacturer_id,
            &manufacturer_data, &manufacturer_data_len);
        if (ret == BT_ERROR_NO_DATA) {
          ret = bt_adapter_le_get_scan_result_manufacturer_data(
              info, BT_ADAPTER_LE_PACKET_ADVERTISING, &manufacturer_id,
              &manufacturer_data, &manufacturer_data_len);
        }
        if (ret == BT_ERROR_NONE) {
          device.manufacturer_data = std::vector<uint8_t>(
              manufacturer_data, manufacturer_data + manufacturer_data_len);
          free(manufacturer_data);
        } else if (ret != BT_ERROR_NO_DATA) {
          LOG_ERROR("Could not obtain manufacturer info from the device %s.",
                    info->remote_address);
          return;
        }

        char **service_ids = nullptr;
        int service_count = 0;
        ret = bt_adapter_le_get_scan_result_service_uuids(
            info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &service_ids,
            &service_count);
        if (ret == BT_ERROR_NO_DATA) {
          ret = bt_adapter_le_get_scan_result_service_uuids(
              info, BT_ADAPTER_LE_PACKET_ADVERTISING, &service_ids,
              &service_count);
        }
        if (ret == BT_ERROR_NONE) {
          for (int i = 0; i < service_count; ++i) {
            device.service_ids.emplace_back(std::string(service_ids[i]));
            free(service_ids[i]);
          }
          free(service_ids);
        } else if (ret != BT_ERROR_NO_DATA) {
          LOG_ERROR("Could not obtain service UUIDs from the device %s.",
                    info->remote_address);
          return;
        }

        bt_adapter_le_service_data_s *service_data = nullptr;
        int service_data_count = 0;
        ret = bt_adapter_le_get_scan_result_service_data_list(
            info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &service_data,
            &service_data_count);
        if (ret == BT_ERROR_NO_DATA) {
          ret = bt_adapter_le_get_scan_result_service_data_list(
              info, BT_ADAPTER_LE_PACKET_ADVERTISING, &service_data,
              &service_data_count);
        }
        if (ret == BT_ERROR_NONE) {
          for (int i = 0; i < service_data_count; ++i) {
            std::string service_id = service_data[i].service_uuid;
            device.service_data[service_id] =
                std::vector<uint8_t>(service_data[i].service_data,
                                     service_data[i].service_data +
                                         service_data[i].service_data_len);
          }
          bt_adapter_le_free_service_data_list(service_data,
                                               service_data_count);
        } else if (ret != BT_ERROR_NO_DATA) {
          LOG_ERROR("Could not obtain service data from the device %s.",
                    info->remote_address);
          return;
        }

        if (self->device_scan_callback_) {
          self->device_scan_callback_(device);
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
  if (connection_state_change_callback_) {
    connection_state_change_callback_(device_id, ConnectionState::kConnecting);
  }
  return true;
}

bool BleTizen::DisconnectFromDevice(const std::string &device_id) {
  last_error_ = bt_gatt_disconnect(device_id.c_str());
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  if (connection_state_change_callback_) {
    connection_state_change_callback_(device_id,
                                      ConnectionState::kDisconnecting);
  }
  return true;
}

bool BleTizen::SetConnectionStateChangeCallback(
    ConnectionStateChangeCallback on_change, ErrorCallback on_error) {
  last_error_ = bt_gatt_set_connection_state_changed_cb(
      [](int result, bool connected, const char *remote_address,
         void *user_data) {
        BleTizen *self = static_cast<BleTizen *>(user_data);
        if (result != BT_ERROR_NONE && result != BT_ERROR_ALREADY_DONE) {
          LOG_ERROR("The connection request failed: %s",
                    get_error_message(result));
          if (self->connection_error_callback_) {
            self->connection_error_callback_(result, get_error_message(result));
          }
        } else {
          std::string device_id = std::string(remote_address);
          ConnectionState state = connected ? ConnectionState::kConnected
                                            : ConnectionState::kDisconnected;
          if (self->connection_state_change_callback_) {
            self->connection_state_change_callback_(device_id, state);
          }
        }
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  connection_state_change_callback_ = std::move(on_change);
  connection_error_callback_ = std::move(on_error);
  return true;
}

bool BleTizen::UnsetConnectionStateChangeCallback() {
  last_error_ = bt_gatt_unset_connection_state_changed_cb();
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  connection_state_change_callback_ = nullptr;
  connection_error_callback_ = nullptr;
  return true;
}

std::optional<std::vector<DiscoveredService>> BleTizen::DiscoverServices(
    const std::string &device_id) {
  std::vector<DiscoveredService> discovered_services;

  bt_gatt_client_h device_handle = nullptr;
  last_error_ = bt_gatt_client_create(device_id.c_str(), &device_handle);
  if (last_error_ != BT_ERROR_NONE) {
    return std::nullopt;
  }

  last_error_ = bt_gatt_client_foreach_services(
      device_handle,
      [](int total, int index, bt_gatt_h service_handle, void *user_data) {
        auto *discovered_services =
            static_cast<std::vector<DiscoveredService> *>(user_data);
        AddDiscoveredService(service_handle, discovered_services);
        return true;
      },
      &discovered_services);
  if (last_error_ != BT_ERROR_NONE) {
    return std::nullopt;
  }

  bt_gatt_client_destroy(device_handle);
  return discovered_services;
}

bool BleTizen::ReadCharacteristic(
    const QualifiedCharacteristic &characteristic) {
  // TODO(swift-kim): Is the operation thread-safe?
  last_error_ = bt_gatt_client_read_value(
      characteristic.handle(),
      [](int result, bt_gatt_h request_handle, void *user_data) {
        BleTizen *self = static_cast<BleTizen *>(user_data);
        if (result != BT_ERROR_NONE) {
          LOG_ERROR("The read operation resulted in an error: %s",
                    get_error_message(result));
          if (self->characteristic_read_error_callback_) {
            self->characteristic_read_error_callback_(
                result, get_error_message(result));
          }
          return;
        }

        std::shared_ptr<QualifiedCharacteristic> characteristic =
            self->FindCharacteristic(request_handle);
        if (self->characteristic_read_callback_ && characteristic) {
          char *value = nullptr;
          int length = 0;
          int ret = bt_gatt_get_value(request_handle, &value, &length);
          if (ret != BT_ERROR_NONE) {
            LOG_ERROR("Failed to retrieve a value: %s", get_error_message(ret));
            return;
          }
          std::vector<uint8_t> bytes(value, value + length);
          free(value);

          self->characteristic_read_callback_(*characteristic, bytes);
        }
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  return true;
}

bool BleTizen::WriteCharacteristic(
    const QualifiedCharacteristic &characteristic,
    const std::vector<uint8_t> &value, CharacteristicWriteCallback on_write,
    ErrorCallback on_error) {
  last_error_ = bt_gatt_set_value(characteristic.handle(),
                                  reinterpret_cast<const char *>(value.data()),
                                  static_cast<int>(value.size()));
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }

  if (characteristic_write_callback_) {
    LOG_ERROR("A write operation is already in progress.");
    last_error_ = TIZEN_ERROR_ALREADY_IN_PROGRESS;
    return false;
  }
  characteristic_write_callback_ = std::move(on_write);
  characteristic_write_error_callback_ = std::move(on_error);

  last_error_ = bt_gatt_client_write_value(
      characteristic.handle(),
      [](int result, bt_gatt_h request_handle, void *user_data) {
        BleTizen *self = static_cast<BleTizen *>(user_data);
        if (result != BT_ERROR_NONE) {
          LOG_ERROR("The write operation resulted in an error: %s",
                    get_error_message(result));
          if (self->characteristic_write_error_callback_) {
            self->characteristic_write_error_callback_(
                result, get_error_message(result));
          }
        } else {
          std::shared_ptr<QualifiedCharacteristic> characteristic =
              self->FindCharacteristic(request_handle);
          if (self->characteristic_write_callback_ && characteristic) {
            self->characteristic_write_callback_(*characteristic);
          }
        }
        self->characteristic_write_callback_ = nullptr;
        self->characteristic_write_error_callback_ = nullptr;
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  return true;
}

bool BleTizen::ListenNotifications(
    const QualifiedCharacteristic &characteristic) {
  last_error_ = bt_gatt_client_set_characteristic_value_changed_cb(
      characteristic.handle(),
      [](bt_gatt_h handle, char *value, int len, void *user_data) {
        BleTizen *self = static_cast<BleTizen *>(user_data);
        std::shared_ptr<QualifiedCharacteristic> characteristic =
            self->FindCharacteristic(handle);
        if (self->characteristic_read_callback_ && characteristic) {
          std::vector<uint8_t> bytes(value, value + len);
          self->characteristic_read_callback_(*characteristic, bytes);
        }
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  return true;
}

bool BleTizen::StopNotifications(
    const QualifiedCharacteristic &characteristic) {
  last_error_ = bt_gatt_client_unset_characteristic_value_changed_cb(
      characteristic.handle());
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  UnregisterCharacteristic(characteristic.handle());
  return true;
}

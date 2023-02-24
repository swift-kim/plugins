// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ble_device.h"

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

BleDevice::BleDevice(const std::string &device_id) : device_id_(device_id) {
  last_error_ = bt_gatt_client_create(device_id.c_str(), &handle_);
  if (last_error_ != BT_ERROR_NONE) {
    LOG_ERROR("Failed to create a client handle: %s",
              get_error_message(last_error_));
    return;
  }
}

BleDevice::~BleDevice() {
  if (handle_) {
    bt_gatt_client_destroy(handle_);
  }
}

std::vector<DiscoveredService> BleDevice::DiscoverServices() {
  std::vector<DiscoveredService> discovered_services;
  last_error_ = bt_gatt_client_foreach_services(
      handle_,
      [](int total, int index, bt_gatt_h service_handle, void *user_data) {
        auto *discovered_services =
            static_cast<std::vector<DiscoveredService> *>(user_data);
        AddDiscoveredService(service_handle, discovered_services);
        return true;
      },
      &discovered_services);
  if (last_error_ != BT_ERROR_NONE) {
    LOG_ERROR("Unable to look up services: %s", get_error_message(last_error_));
  }
  return discovered_services;
}

bool BleDevice::ReadCharacteristic(
    const QualifiedCharacteristic &characteristic) {
  // TODO(swift-kim): Should the operation be thread-safe?
  last_error_ = bt_gatt_client_read_value(
      characteristic.handle(),
      [](int result, bt_gatt_h request_handle, void *user_data) {
        BleDevice *self = static_cast<BleDevice *>(user_data);
        if (result != BT_ERROR_NONE) {
          LOG_ERROR("The read operation resulted in an error: %s",
                    get_error_message(result));
          if (self->characteristic_event_error_callback_) {
            self->characteristic_event_error_callback_(
                result, get_error_message(result));
          }
          return;
        }

        std::shared_ptr<QualifiedCharacteristic> characteristic =
            self->FindCharacteristic(request_handle);
        if (self->characteristic_event_callback_ && characteristic) {
          char *value = nullptr;
          int length = 0;
          int ret = bt_gatt_get_value(request_handle, &value, &length);
          if (ret != BT_ERROR_NONE) {
            LOG_ERROR("Failed to retrieve a value: %s", get_error_message(ret));
            return;
          }
          std::vector<uint8_t> bytes(value, value + length);
          free(value);

          self->characteristic_event_callback_(*characteristic, bytes);
        }
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  return true;
}

bool BleDevice::WriteCharacteristic(
    const QualifiedCharacteristic &characteristic,
    const std::vector<uint8_t> &value, CharacteristicWriteCallback on_complete,
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

  last_error_ = bt_gatt_client_write_value(
      characteristic.handle(),
      [](int result, bt_gatt_h request_handle, void *user_data) {
        BleDevice *self = static_cast<BleDevice *>(user_data);
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
  characteristic_write_callback_ = std::move(on_complete);
  characteristic_write_error_callback_ = std::move(on_error);

  return true;
}

bool BleDevice::ListenNotifications(
    const QualifiedCharacteristic &characteristic) {
  last_error_ = bt_gatt_client_set_characteristic_value_changed_cb(
      characteristic.handle(),
      [](bt_gatt_h handle, char *value, int len, void *user_data) {
        BleDevice *self = static_cast<BleDevice *>(user_data);
        std::shared_ptr<QualifiedCharacteristic> characteristic =
            self->FindCharacteristic(handle);
        if (self->characteristic_event_callback_ && characteristic) {
          std::vector<uint8_t> bytes(value, value + len);
          self->characteristic_event_callback_(*characteristic, bytes);
        }
      },
      this);
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  return true;
}

bool BleDevice::StopNotifications(
    const QualifiedCharacteristic &characteristic) {
  last_error_ = bt_gatt_client_unset_characteristic_value_changed_cb(
      characteristic.handle());
  if (last_error_ != BT_ERROR_NONE) {
    return false;
  }
  UnregisterCharacteristic(characteristic.handle());
  return true;
}

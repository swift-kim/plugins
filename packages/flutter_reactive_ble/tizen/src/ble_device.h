// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_PLUGIN_BLE_DEVICE_H_
#define FLUTTER_PLUGIN_BLE_DEVICE_H_

#include <bluetooth_type.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "qualified_characteristic.h"

struct DiscoveredCharacteristic {
  Uuid characteristic_id;
  Uuid service_id;
  bool is_readable = false;
  bool is_writable_with_response = false;
  bool is_writable_without_response = false;
  bool is_notifiable = false;
  bool is_indicatable = false;
};

struct DiscoveredService {
  Uuid service_id;
  std::vector<Uuid> characteristic_ids;
  std::vector<DiscoveredCharacteristic> characteristics;
  std::vector<DiscoveredService> included_services;
};

typedef std::function<void(const QualifiedCharacteristic& characteristic,
                           const std::vector<uint8_t>& bytes)>
    CharacteristicEventCallback;

typedef std::function<void(const QualifiedCharacteristic& characteristic)>
    CharacteristicWriteCallback;

typedef std::function<void(int error_code, const std::string& error_message)>
    ErrorCallback;

// A wrapper around the bt_gatt_client module.
class BleDevice {
 public:
  explicit BleDevice(const std::string& device_id);
  ~BleDevice();

  // Discovers services adverstised by this device.
  std::vector<DiscoveredService> DiscoverServices();

  // Reads a value from the specified characteristic.
  bool ReadCharacteristic(const QualifiedCharacteristic& characteristic);

  // Writes a value to the specified characteristic.
  bool WriteCharacteristic(const QualifiedCharacteristic& characteristic,
                           const std::vector<uint8_t>& value,
                           CharacteristicWriteCallback on_complete,
                           ErrorCallback on_error);

  // Subscribes to updates from the specified characteristic.
  bool ListenNotifications(const QualifiedCharacteristic& characteristic);

  // Unsubscribes from the characteristic.
  bool StopNotifications(const QualifiedCharacteristic& characteristic);

  void SetNotificationCallbacks(CharacteristicEventCallback on_event,
                                ErrorCallback on_error) {
    characteristic_event_callback_ = std::move(on_event);
    characteristic_event_error_callback_ = std::move(on_error);
  }

  std::shared_ptr<QualifiedCharacteristic> FindCharacteristicById(
      const Uuid& service_id, const Uuid& characteristic_id) {
    for (const auto& characteristic : characteristics_) {
      if (service_id == characteristic->service_id() &&
          characteristic_id == characteristic->characteristic_id()) {
        return characteristic;
      }
    }
    return RegisterCharacteristic(service_id, characteristic_id);
  }

  std::string device_id() const { return device_id_; }

  bt_gatt_client_h handle() const { return handle_; }

  int GetLastError() { return last_error_; }

  std::string GetLastErrorString() { return get_error_message(last_error_); }

 private:
  std::shared_ptr<QualifiedCharacteristic> RegisterCharacteristic(
      const Uuid& service_id, const Uuid& characteristic_id) {
    auto characteristic = std::make_shared<QualifiedCharacteristic>(
        this, service_id, characteristic_id);
    if (!characteristic->handle()) {
      return nullptr;
    }
    return characteristics_.emplace_back(std::move(characteristic));
  }

  std::shared_ptr<QualifiedCharacteristic> FindCharacteristic(
      bt_gatt_h handle) {
    for (const auto& characteristic : characteristics_) {
      if (handle == characteristic->handle()) {
        return characteristic;
      }
    }
    return nullptr;
  }

  void UnregisterCharacteristic(bt_gatt_h handle) {
    auto iter = std::find_if(
        characteristics_.begin(), characteristics_.end(),
        [handle](std::shared_ptr<QualifiedCharacteristic> instance) {
          return instance->handle() == handle;
        });
    if (iter != characteristics_.end()) {
      characteristics_.erase(iter);
    }
  }

  std::string device_id_;
  bt_gatt_client_h handle_ = nullptr;

  std::vector<std::shared_ptr<QualifiedCharacteristic>> characteristics_;
  CharacteristicEventCallback characteristic_event_callback_;
  ErrorCallback characteristic_event_error_callback_;
  CharacteristicWriteCallback characteristic_write_callback_;
  ErrorCallback characteristic_write_error_callback_;

  int last_error_ = BT_ERROR_NONE;
};

#endif  // FLUTTER_PLUGIN_BLE_DEVICE_H_

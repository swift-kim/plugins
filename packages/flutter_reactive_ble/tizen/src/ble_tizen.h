// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_PLUGIN_BLE_TIZEN_H_
#define FLUTTER_PLUGIN_BLE_TIZEN_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.h"

struct DiscoveredDevice {
  std::string device_id;
  std::string name;
  std::vector<uint8_t> manufacturer_data;
  std::vector<Uuid> service_ids;
  std::map<Uuid, std::vector<std::uint8_t>> service_data;
  int32_t rssi = 0;
};

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

enum class BleStatus {
  kDisabled,
  kEnabled,
  kNotSupported,
};

enum class ConnectionState {
  kConnecting,
  kConnected,
  kDisconnecting,
  kDisconnected,
};

typedef std::function<void(BleStatus)> BleStatusChangedCallback;

typedef std::function<void(std::unique_ptr<DiscoveredDevice>)>
    DeviceScanCallback;

typedef std::function<void(const std::string& device_id, ConnectionState)>
    ConnectionStateChangedCallback;

typedef std::function<void(const QualifiedCharacteristic& characteristic,
                           const std::vector<uint8_t>&)>
    CharacteristicValueChangedCallback;

typedef std::function<void(const QualifiedCharacteristic& characteristic)>
    CharacteristicValueWriteCallback;

class BleTizen {
 public:
  ~BleTizen() = default;

  // Initializes the bluetooth low enery module.
  bool Initialize();

  // Clean ups the bluetooth low enery module.
  bool Deinitialize();

  std::optional<BleStatus> GetBleStatus();

  bool SetBleStatusChangedCallback(BleStatusChangedCallback callback);

  bool UnsetBleStatusChangedCallback();

  // Scans for surrounding devices.
  bool Scan(const std::vector<Uuid>& service_ids);

  // Stops scanning for devices.
  bool StopScan();

  void SetDeviceScanCallback(DeviceScanCallback callback) {
    device_scan_callback_ = std::move(callback);
  }

  // Connects to device with |device_id|.
  bool ConnectToDevice(const std::string& device_id);

  // Disconnects from device with |device_id|.
  bool DisconnectFromDevice(const std::string& device_id);

  bool SetConnectionStateChangedCallback(
      ConnectionStateChangedCallback callback);

  bool UnsetConnectionStateChangedCallback();

  // Reads value from characteristic.
  bool ReadCharacteristic(const QualifiedCharacteristic& characteristic);

  // Writes value to characteristic.
  bool WriteCharacteristic(const QualifiedCharacteristic& characteristic,
                           const std::vector<uint8_t>& value,
                           CharacteristicValueWriteCallback callback);

  // Listens for notification from characteristic.
  bool ListenNotifications(const QualifiedCharacteristic& characteristic);

  // Stops listening for notifications from characteristic.
  bool StopNotifications(const QualifiedCharacteristic& characteristic);

  void SetNotificationCallback(CharacteristicValueChangedCallback callback) {
    notification_callback_ = std::move(callback);
  }

  // Discovers services adverstised by the device.
  std::optional<std::vector<DiscoveredService>> DiscoverServices(
      const std::string& device_id);

  std::shared_ptr<QualifiedCharacteristic> AddCharacteristic(
      const std::string& device_id, const Uuid& service_id,
      const Uuid& characteristic_id) {
    auto instance = std::make_shared<QualifiedCharacteristic>(
        device_id, service_id, characteristic_id);
    return characteristics_.emplace_back(std::move(instance));
  }

  std::shared_ptr<QualifiedCharacteristic> FindCharacteristicById(
      const std::string& device_id, const Uuid& service_id,
      const Uuid& characteristic_id) {
    for (const auto& characteristic : characteristics_) {
      if (device_id == characteristic->device_id() &&
          service_id == characteristic->service_id() &&
          characteristic_id == characteristic->characteristic_id()) {
        return characteristic;
      }
    }
    return nullptr;
  }

  int GetLastError() { return last_error_; }

  std::string GetLastErrorString() { return get_error_message(last_error_); }

 private:
  std::shared_ptr<QualifiedCharacteristic> FindCharacteristic(
      bt_gatt_h handle) {
    for (const auto& characteristic : characteristics_) {
      if (handle == characteristic->handle()) {
        return characteristic;
      }
    }
    return nullptr;
  }

  void RemoveCharacteristic(bt_gatt_h handle) {
    auto iter = std::find_if(
        characteristics_.begin(), characteristics_.end(),
        [handle](std::shared_ptr<QualifiedCharacteristic> instance) {
          return instance->handle() == handle;
        });
    if (iter != characteristics_.end()) {
      characteristics_.erase(iter);
    }
  }

  bool is_scanning_ = false;

  BleStatusChangedCallback ble_status_changed_callback_;
  DeviceScanCallback device_scan_callback_;
  ConnectionStateChangedCallback connection_state_changed_callback_;
  CharacteristicValueWriteCallback characteristic_value_write_callback_;
  CharacteristicValueChangedCallback notification_callback_;

  std::vector<std::shared_ptr<QualifiedCharacteristic>> characteristics_;
  std::vector<std::unique_ptr<ScanFilter>> filters_;

  int last_error_;
};

#endif  // FLUTTER_PLUGIN_BLE_TIZEN_H_

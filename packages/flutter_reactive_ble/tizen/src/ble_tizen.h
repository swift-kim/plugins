// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_PLUGIN_BLE_TIZEN_H_
#define FLUTTER_PLUGIN_BLE_TIZEN_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.h"

struct ServiceDataEntry {
  Uuid service_id;
  std::vector<std::uint8_t> data;
};

struct DeviceScanInfo {
  std::string device_id;

  std::string name;

  std::vector<ServiceDataEntry> service_data;

  std::vector<uint8_t> manufacturer_data;

  std::vector<Uuid> service_ids;

  int32_t rssi;
};

struct ServiceWithCharacteristics {
  Uuid service_id;
  std::vector<Uuid> characteristics;
};

struct DiscoveredCharacteristic {
  Uuid characteristic_id;
  Uuid service_id;
  bool is_readable;
  bool is_writable_with_response;
  bool is_writable_without_response;
  bool is_notifiable;
  bool is_indicatable;
};

struct DiscoveredService {
  Uuid service_id;
  std::vector<Uuid> characteristic_ids;
  std::vector<DiscoveredCharacteristic> characteristics;
  std::vector<DiscoveredService> included_services;
};

class ErrorReportMixin {
 public:
  int error_code() const { return error_code_; }

  std::string error_message() const;

 protected:
  int error_code_;
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

// A device scan callback.
typedef std::function<void(std::unique_ptr<DeviceScanInfo>)> DeviceScanCallback;

// A ble connection state changed callback.
typedef std::function<void(const std::string& device_id, ConnectionState)>
    ConnectionStateChangedCallback;

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
  bool ReadCharacteristics(const QualifiedCharacteristic& characteristic);

  // Writes value to characteristic.
  bool WriteCharacteristics(const QualifiedCharacteristic& characteristic,
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

  int GetLastError() { return last_error_; }

  std::string GetLastErrorString() { return get_error_message(last_error_); }

 private:
  bool is_scanning_ = false;

  BleStatusChangedCallback ble_status_changed_callback_;
  DeviceScanCallback device_scan_callback_;
  ConnectionStateChangedCallback connection_state_changed_callback_;
  CharacteristicValueChangedCallback notification_callback_;

  std::unordered_map<QualifiedCharacteristic, CharacteristicHandle,
                     CharacteristicHasher>
      native_handles_;

  std::vector<std::unique_ptr<ScanFilter>> filters_;

  int last_error_;
};

#endif  // FLUTTER_PLUGIN_BLE_TIZEN_H_

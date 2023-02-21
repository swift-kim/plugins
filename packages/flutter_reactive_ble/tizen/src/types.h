// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_PLUGIN_TYPES_H_
#define FLUTTER_PLUGIN_TYPES_H_

#include <bluetooth.h>
#include <bluetooth_type.h>

#include <functional>
#include <string>

#include "log.h"

typedef std::string Uuid;

// A value class that represents a BLE characteristic.
class QualifiedCharacteristic {
 public:
  QualifiedCharacteristic(const std::string &device_id, const Uuid &service_id,
                          const Uuid &characteristic_id)
      : device_id_(device_id),
        service_id_(service_id),
        characteristic_id_(characteristic_id) {}

  std::string device_id() const { return device_id_; }

  Uuid service_id() const { return service_id_; }

  Uuid characteristic_id() const { return characteristic_id_; }

  bool operator==(const QualifiedCharacteristic &other) const {
    return device_id_ == other.device_id_ && service_id_ == other.service_id_ &&
           characteristic_id_ == other.characteristic_id_;
  }

 private:
  std::string device_id_;
  Uuid service_id_;
  Uuid characteristic_id_;
};

struct CharacteristicHasher {
  std::size_t operator()(const QualifiedCharacteristic &characteristic) const {
    return std::hash<std::string>()(characteristic.device_id()) ^
           std::hash<std::string>()(characteristic.service_id()) ^
           std::hash<std::string>()(characteristic.characteristic_id());
  }
};

typedef std::function<void(QualifiedCharacteristic characteristic,
                           const std::vector<uint8_t> &)>
    CharacteristicValueChangedCallback;
typedef std::function<void(QualifiedCharacteristic characteristic,
                           const std::vector<uint8_t> &)>
    CharacteristicValueReadCallback;
typedef std::function<void(QualifiedCharacteristic characteristic)>
    CharacteristicValueWriteCallback;

/// A wrapper around native GATT handles associated with a BLE characteristic.
class CharacteristicHandle {
 public:
  explicit CharacteristicHandle(const QualifiedCharacteristic &characteristic);
  ~CharacteristicHandle();

  // Prevent copying.
  CharacteristicHandle(const CharacteristicHandle &handle) = delete;
  CharacteristicHandle &operator=(const CharacteristicHandle &handle) = delete;

  CharacteristicHandle(CharacteristicHandle &&handle)
      : device_handle_(handle.device_handle_),
        service_handle_(handle.service_handle_),
        characteristic_handle_(handle.characteristic_handle_),
        characteristic_value_changed_callback_(
            std::move(handle.characteristic_value_changed_callback_)),
        characteristic_value_read_callback_(
            std::move(handle.characteristic_value_read_callback_)),
        characteristic_value_write_callback_(
            std::move(handle.characteristic_value_write_callback_)){};
  CharacteristicHandle &operator=(CharacteristicHandle &&handle) = delete;

  QualifiedCharacteristic ToQualifiedCharacteristic() const {
    return QualifiedCharacteristic(device_id(), service_id(),
                                   characteristic_id());
  }

  std::string device_id() const { return GetDeviceId(device_handle_); }

  Uuid service_id() const { return GetUuid(service_handle_); }

  Uuid characteristic_id() const { return GetUuid(characteristic_handle_); }

  bt_gatt_h characteristic_handle() const { return characteristic_handle_; }

  void SetValueChangedCallback(CharacteristicValueChangedCallback callback) {
    characteristic_value_changed_callback_ = callback;
  }

  void SetValueReadCallback(CharacteristicValueReadCallback callback) {
    characteristic_value_read_callback_ = callback;
  }

  void SetValueWroteCallback(CharacteristicValueWriteCallback callback) {
    characteristic_value_write_callback_ = callback;
  }

  void NotifyValueChanged(const std::vector<uint8_t> &value) const {
    if (characteristic_value_changed_callback_) {
      characteristic_value_changed_callback_(ToQualifiedCharacteristic(),
                                             value);
    }
  }

  void NotifyValueRead(const std::vector<uint8_t> &value) const {
    if (characteristic_value_read_callback_) {
      characteristic_value_read_callback_(ToQualifiedCharacteristic(), value);
    }
  }

  void NotifyValueWrote() const {
    if (characteristic_value_write_callback_) {
      characteristic_value_write_callback_(ToQualifiedCharacteristic());
    }
  }

  bool IsReadable() const {
    int properties = 0;
    bt_gatt_characteristic_get_properties(characteristic_handle_, &properties);
    return properties & BT_GATT_PROPERTY_READ;
  }

 private:
  bt_gatt_client_h device_handle_ = nullptr;
  bt_gatt_h service_handle_ = nullptr;
  bt_gatt_h characteristic_handle_ = nullptr;

  CharacteristicValueChangedCallback characteristic_value_changed_callback_;
  CharacteristicValueReadCallback characteristic_value_read_callback_;
  CharacteristicValueWriteCallback characteristic_value_write_callback_;

  static std::string GetDeviceId(const bt_gatt_client_h &device_handle);
  static Uuid GetUuid(const bt_gatt_h &gatt_handle);
};

// A wrapper around a native bt_scan_filter_h handle.
class ScanFilter {
 public:
  explicit ScanFilter(const Uuid &service_id);
  virtual ~ScanFilter();

  // Prevent copying.
  ScanFilter(const ScanFilter &handle) = delete;
  ScanFilter &operator=(const ScanFilter &handle) = delete;

  ScanFilter(ScanFilter &&other) : handle_(other.handle_) {}
  ScanFilter &operator=(ScanFilter &&other) {
    handle_ = other.handle_;
    return *this;
  }

  bool Register();

  bool Unregister();

  static bool UnregisterAll();

  bt_scan_filter_h handle() const { return handle_; }

 private:
  bt_scan_filter_h handle_;
};

#endif  // FLUTTER_PLUGIN_TYPES_H_

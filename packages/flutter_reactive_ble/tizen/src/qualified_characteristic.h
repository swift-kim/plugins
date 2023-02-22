// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_PLUGIN_QUALIFIED_CHARACTERISTIC_H_
#define FLUTTER_PLUGIN_QUALIFIED_CHARACTERISTIC_H_

#include <bluetooth_type.h>

#include <string>
#include <unordered_map>

typedef std::string Uuid;

class QualifiedCharacteristic {
 public:
  explicit QualifiedCharacteristic(const std::string &device_id,
                                   const Uuid &service_id,
                                   const Uuid &characteristic_id);
  ~QualifiedCharacteristic();

  // Prevent copying.
  QualifiedCharacteristic(QualifiedCharacteristic const &) = delete;
  QualifiedCharacteristic &operator=(QualifiedCharacteristic const &) = delete;

  bool IsReadable() const;

  std::string device_id() const { return device_id_; }

  Uuid service_id() const { return service_id_; }

  Uuid characteristic_id() const { return characteristic_id_; }

  bt_gatt_h handle() const { return handle_; }

 private:
  std::string device_id_;
  Uuid service_id_;
  Uuid characteristic_id_;

  // TODO(swift-kim): Avoid manual reference counting.
  static inline std::unordered_map<std::string, bt_gatt_client_h>
      device_handles_;
  static inline std::unordered_map<std::string, int32_t>
      device_reference_counts_;

  bt_gatt_h handle_ = nullptr;
};

#endif  // FLUTTER_PLUGIN_QUALIFIED_CHARACTERISTIC_H_

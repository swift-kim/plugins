// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qualified_characteristic.h"

#include <bluetooth.h>

#include "log.h"

QualifiedCharacteristic::QualifiedCharacteristic(const std::string &device_id,
                                                 const Uuid &service_id,
                                                 const Uuid &characteristic_id)
    : device_id_(device_id),
      service_id_(service_id),
      characteristic_id_(characteristic_id) {
  bt_gatt_client_h device_handle = nullptr;
  // A device handle cannot be created twice for the same device ID.
  auto iter = device_handles_.find(device_id);
  if (iter == device_handles_.end()) {
    int ret = bt_gatt_client_create(device_id.c_str(), &device_handle);
    if (ret != BT_ERROR_NONE) {
      LOG_ERROR("Failed to create a client handle: %s", get_error_message(ret));
      return;
    }
    device_handles_[device_id] = device_handle;
    device_reference_counts_[device_id] = 1;
  } else {
    device_handle = iter->second;
    device_reference_counts_[device_id]++;
  }

  bt_gatt_h service_handle = nullptr;
  int ret = bt_gatt_client_get_service(device_handle, service_id.c_str(),
                                       &service_handle);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to create a service handle: %s", get_error_message(ret));
    return;
  }

  ret = bt_gatt_service_get_characteristic(service_handle,
                                           characteristic_id.c_str(), &handle_);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to create a characteristic handle: %s",
              get_error_message(ret));
    return;
  }
}

QualifiedCharacteristic::~QualifiedCharacteristic() {
  auto iter = device_handles_.find(device_id_);
  if (iter != device_handles_.end()) {
    device_reference_counts_[device_id_]--;
    if (device_reference_counts_[device_id_] == 0) {
      bt_gatt_client_destroy(iter->second);
      device_handles_.erase(device_id_);
      device_reference_counts_.erase(device_id_);
    }
  }
}

bool QualifiedCharacteristic::IsReadable() const {
  int properties = 0;
  int ret = bt_gatt_characteristic_get_properties(handle_, &properties);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Unable to get characteristic properties: %s",
              get_error_message(ret));
    return false;
  }
  return properties & BT_GATT_PROPERTY_READ;
}

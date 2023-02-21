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
  int ret = bt_gatt_client_create(device_id.c_str(), &device_);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to create a client handle: %s", get_error_message(ret));
    return;
  }

  ret = bt_gatt_client_get_service(device_, service_id.c_str(), &service_);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to create a service handle: %s", get_error_message(ret));
    return;
  }

  ret = bt_gatt_service_get_characteristic(service_, characteristic_id.c_str(),
                                           &characteristic_);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to create a characteristic handle: %s",
              get_error_message(ret));
    return;
  }
}

QualifiedCharacteristic::~QualifiedCharacteristic() {
  if (device_) {
    bt_gatt_client_destroy(device_);
  }
}

bool QualifiedCharacteristic::IsReadable() const {
  int properties = 0;
  int ret = bt_gatt_characteristic_get_properties(characteristic_, &properties);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Unable to get characteristic properties: %s",
              get_error_message(ret));
    return false;
  }
  return properties & BT_GATT_PROPERTY_READ;
}

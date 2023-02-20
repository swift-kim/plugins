// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

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
    LOG_ERROR("Failed to get characteristic properties: %s",
              get_error_message(ret));
    return false;
  }
  return properties & BT_GATT_PROPERTY_READ;
}

std::string QualifiedCharacteristic::GetDeviceId(
    bt_gatt_client_h device_handle) {
  char *address_cstr = nullptr;
  int ret = bt_gatt_client_get_remote_address(device_handle, &address_cstr);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to get a remote device address: %s",
              get_error_message(ret));
    return std::string();
  }
  std::string address = std::string(address_cstr);
  free(address_cstr);
  return address;
}

Uuid QualifiedCharacteristic::GetUuid(bt_gatt_h gatt_handle) {
  char *uuid_cstr = nullptr;
  int ret = bt_gatt_get_uuid(gatt_handle, &uuid_cstr);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to retrieve a UUID: %s", get_error_message(ret));
    return std::string();
  }
  std::string uuid = std::string(uuid_cstr);
  free(uuid_cstr);
  return uuid;
}

ScanFilter::ScanFilter(const Uuid &service_id) {
  int ret = bt_adapter_le_scan_filter_create(&handle_);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to create a scan filter handle.");
    return;
  }
  ret = bt_adapter_le_scan_filter_set_service_uuid(handle_, service_id.c_str());
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Failed to set a service UUID for a scan filter.");
    return;
  }
};

ScanFilter::~ScanFilter() {
  if (handle_) {
    bt_adapter_le_scan_filter_destroy(handle_);
  }
};

bool ScanFilter::Register() {
  int ret = bt_adapter_le_scan_filter_register(handle_);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Filter registration failed: %s", get_error_message(ret));
    return false;
  }
  return true;
};

bool ScanFilter::Unregister() {
  int ret = bt_adapter_le_scan_filter_unregister(handle_);
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Filter unregistration failed: %s", get_error_message(ret));
    return false;
  }
  return true;
};

bool ScanFilter::UnregisterAll() {
  int ret = bt_adapter_le_scan_filter_unregister_all();
  if (ret != BT_ERROR_NONE) {
    LOG_ERROR("Filter unregistration failed: %s", get_error_message(ret));
    return false;
  }
  return true;
}

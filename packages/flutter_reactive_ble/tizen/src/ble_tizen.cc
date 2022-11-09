#include "ble_tizen.h"

#include <bluetooth.h>
#include <bluetooth_type.h>

#include "log.h"

namespace {
/// Gets the uuid of GATT handle.
uuid GetUuid(const bt_gatt_h &gatt_handle) {
  char *uuid_cstr;
  bt_gatt_get_uuid(gatt_handle, &uuid_cstr);
  std::string uuid = std::string(uuid_cstr);
  free(uuid_cstr);
  return uuid;
}

/// Gets the address of device.
std::string GetDeviceAddress(const bt_gatt_client_h &device_handle) {
  char *device_address_cstr;
  bt_gatt_client_get_remote_address(device_handle, &device_address_cstr);
  std::string device_address = std::string(device_address_cstr);
  free(device_address_cstr);
  return device_address;
}

void GetDiscoveredService(bt_gatt_h service_handle,
                          std::vector<DiscoveredService> *discovered_services) {
  uuid service_uuid = GetUuid(service_handle);
  DiscoveredService discovered_service;
  discovered_service.service_id = service_uuid;
  bt_gatt_service_foreach_included_services(
      service_handle,
      [](int total, int index, bt_gatt_h included_service_handle,
         void *user_data) {
        DiscoveredService *discovered_service =
            static_cast<DiscoveredService *>(user_data);
        GetDiscoveredService(included_service_handle,
                             &discovered_service->included_services);
        return true;
      },
      &discovered_service);
  bt_gatt_service_foreach_characteristics(
      service_handle,
      [](int total, int index, bt_gatt_h characteristic_handle,
         void *user_data) {
        uuid characteristic_uuid = GetUuid(characteristic_handle);
        int properties;
        bt_gatt_characteristic_get_properties(characteristic_handle,
                                              &properties);

        DiscoveredCharacteristic discovered_characteristic;
        discovered_characteristic.characteristic_id = characteristic_uuid;
        discovered_characteristic.is_readable =
            properties & BT_GATT_PROPERTY_READ;
        discovered_characteristic.is_writable_with_response =
            properties & BT_GATT_PROPERTY_WRITE;
        discovered_characteristic.is_writable_without_response =
            properties & BT_GATT_PROPERTY_WRITE_WITHOUT_RESPONSE;
        discovered_characteristic.is_notifiable =
            properties & BT_GATT_PROPERTY_NOTIFY;
        discovered_characteristic.is_indicatable =
            properties & BT_GATT_PROPERTY_INDICATE;

        DiscoveredService *discovered_service =
            static_cast<DiscoveredService *>(user_data);
        discovered_characteristic.service_id = discovered_service->service_id;
        discovered_service->characteristic_uuids.push_back(characteristic_uuid);
        discovered_service->characteristics.push_back(
            discovered_characteristic);
        return true;
      },
      &discovered_service);

  discovered_services->push_back(discovered_service);
}

}  // namespace

std::string ErrorReportMixin::error_message() const {
  return get_error_message(error_code_);
}

// Wrapper around bt_scan_filter_h handle and its operations.
class FilterImpl : public Filter {
 public:
  explicit FilterImpl(const uuid &service_uuid) {
    bt_adapter_le_scan_filter_create(&handle_);
    bt_adapter_le_scan_filter_set_service_uuid(handle_, service_uuid.c_str());
  };

  virtual ~FilterImpl() { bt_adapter_le_scan_filter_destroy(handle_); };

  // Prevent copying.
  FilterImpl(const FilterImpl &handle) = delete;
  FilterImpl &operator=(const FilterImpl &handle) = delete;

  FilterImpl(FilterImpl &&other) : handle_(std::move(other.handle_)) {}
  FilterImpl &operator=(FilterImpl &&other) {
    handle_ = std::move(other.handle_);
    return *this;
  }

  static int UnregisterAll() {
    return bt_adapter_le_scan_filter_unregister_all();
  }

  // |Filter|
  bool Register() override {
    error_code_ = bt_adapter_le_scan_filter_register(handle_);
    if (error_code_ != BT_ERROR_NONE) {
      LOG_ERROR("Filter registration failed: %s.",
                get_error_message(error_code_));
      return false;
    }
    return true;
  };

  // |Filter|
  bool Unregister() override {
    error_code_ = bt_adapter_le_scan_filter_unregister(handle_);
    if (error_code_ != BT_ERROR_NONE) {
      LOG_ERROR("Filter unregistration failed: %s.",
                get_error_message(error_code_));
      return false;
    }
    return true;
  };

  // |Filter|
  const bt_scan_filter_h &handle() const override { return handle_; }

 private:
  bt_scan_filter_h handle_;
};

BleTizen::~BleTizen() = default;

bool BleTizen::Initialize() {
  error_code_ = bt_initialize();
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  return true;
}

bool BleTizen::Deinitialize() {
  error_code_ = bt_deinitialize();
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  return true;
}

std::optional<BleStatus> BleTizen::GetBleStatus() {
  bt_adapter_state_e adapter_state;
  error_code_ = bt_adapter_get_state(&adapter_state);
  if (error_code_ == BT_ERROR_NONE) {
    return adapter_state == BT_ADAPTER_ENABLED ? BleStatus::kEnabled
                                               : BleStatus::kDisabled;
  } else if (error_code_ == BT_ERROR_NOT_SUPPORTED) {
    return BleStatus::kNotSupported;
  }
  return std::nullopt;
}

bool BleTizen::SetBleStatusChangedCallback(BleStatusChangedCallback callback) {
  error_code_ = bt_adapter_set_state_changed_cb(
      [](int result, bt_adapter_state_e adapter_state, void *user_data) {
        BleTizen *ble_tizen = static_cast<BleTizen *>(user_data);

        if (ble_tizen->ble_status_changed_callback_) {
          ble_tizen->ble_status_changed_callback_(
              adapter_state == BT_ADAPTER_ENABLED ? BleStatus::kEnabled
                                                  : BleStatus::kDisabled);
        }
      },
      this);
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  ble_status_changed_callback_ = callback;
  return true;
}

bool BleTizen::UnsetBleStatusChangedCallback() {
  error_code_ = bt_adapter_unset_state_changed_cb();
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  ble_status_changed_callback_ = nullptr;
  return true;
}

bool BleTizen::Scan(const std::vector<uuid> &service_filters) {
  if (scanning_) {
    StopScan();
  }

  for (const auto &uuid : service_filters) {
    std::unique_ptr<FilterImpl> filter = std::make_unique<FilterImpl>(uuid);
    if (!filter->Register()) {
      error_code_ = filter->error_code();
      return false;
    }
    filters_.emplace_back(std::move(filter));
  }

  // TODO: Handle locks when callbacks are run by different threads.
  error_code_ = bt_adapter_le_start_scan(
      [](int result, bt_adapter_le_device_scan_result_info_s *info,
         void *user_data) {
        BleTizen *ble_tizen = static_cast<BleTizen *>(user_data);
        std::unique_ptr<DeviceScanInfo> device_scan_info =
            std::make_unique<DeviceScanInfo>();

        // Get device address.
        device_scan_info->address = std::string(info->remote_address);

        // Get device name.
        char *name;
        ble_tizen->error_code_ = bt_adapter_le_get_scan_result_device_name(
            info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &name);
        if (ble_tizen->error_code_ == BT_ERROR_NONE) {
          device_scan_info->name = std::string(name);
          free(name);
        } else if (ble_tizen->error_code_ == BT_ERROR_NO_DATA) {
          // Leave the name data empty.
        } else {
          return;
        }

        // Get manufacturer data.
        int manufacturer_id;
        char *manufacturer_data;
        int manufacturer_data_len;
        ble_tizen->error_code_ =
            bt_adapter_le_get_scan_result_manufacturer_data(
                info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &manufacturer_id,
                &manufacturer_data, &manufacturer_data_len);
        if (ble_tizen->error_code_ == BT_ERROR_NONE) {
          device_scan_info->manufacturer_data = std::vector<uint8_t>(
              manufacturer_data, manufacturer_data + manufacturer_data_len);
          free(manufacturer_data);
        } else if (ble_tizen->error_code_ == BT_ERROR_NO_DATA) {
          // Leave the manufacturer data empty.
        } else {
          return;
        }

        // Get service uuids.
        char **service_uuids;
        int service_count;
        ble_tizen->error_code_ = bt_adapter_le_get_scan_result_service_uuids(
            info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &service_uuids,
            &service_count);
        if (ble_tizen->error_code_ == BT_ERROR_NONE) {
          for (int i = 0; i < service_count; ++i) {
            device_scan_info->service_uuids.emplace_back(
                std::string(service_uuids[i]));
            free(service_uuids[i]);
          }
          free(service_uuids);
        } else if (ble_tizen->error_code_ == BT_ERROR_NO_DATA) {
          // Leave the service uuid list empty.
        } else {
          return;
        }

        bt_adapter_le_service_data_s *service_data;
        int service_data_count;
        // Get service data.
        ble_tizen->error_code_ =
            bt_adapter_le_get_scan_result_service_data_list(
                info, BT_ADAPTER_LE_PACKET_SCAN_RESPONSE, &service_data,
                &service_data_count);
        if (ble_tizen->error_code_ == BT_ERROR_NONE) {
          for (int i = 0; i < service_data_count; ++i) {
            ServiceDataEntry service_data_entry;
            service_data_entry.service_id = service_data[i].service_uuid;
            service_data_entry.data =
                std::vector<uint8_t>(service_data[i].service_data,
                                     service_data[i].service_data +
                                         service_data[i].service_data_len);
            device_scan_info->service_data.emplace_back(service_data_entry);
          }
          bt_adapter_le_free_service_data_list(service_data,
                                               service_data_count);

        } else if (ble_tizen->error_code_ == BT_ERROR_NO_DATA) {
          // Leave the service data list empty.
        } else {
          return;
        }

        device_scan_info->rssi = info->rssi;

        if (ble_tizen->device_scan_callback_) {
          ble_tizen->device_scan_callback_(std::move(device_scan_info));
        }
      },
      this);
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  scanning_ = true;
  return true;
}

bool BleTizen::StopScan() {
  if (!scanning_) {
    return true;
  }
  error_code_ = bt_adapter_le_stop_scan();
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  error_code_ = FilterImpl::UnregisterAll();
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  filters_.clear();

  scanning_ = false;
  return true;
}

void BleTizen::SetDeviceScanCallback(DeviceScanCallback callback) {
  device_scan_callback_ = callback;
}

void BleTizen::UnsetDeviceScanCallback() { device_scan_callback_ = nullptr; }

bool BleTizen::ConnectToDevice(const std::string &device_address) {
  error_code_ = bt_gatt_connect(device_address.c_str(), false);
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  if (connection_state_changed_callback_) {
    connection_state_changed_callback_(device_address,
                                       ConnectionState::kConnecting);
  }
  return true;
}

bool BleTizen::DisconnectFromDevice(const std::string &device_address) {
  error_code_ = bt_gatt_disconnect(device_address.c_str());
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  if (connection_state_changed_callback_) {
    connection_state_changed_callback_(device_address,
                                       ConnectionState::kDisconnecting);
  }
  return true;
}

bool BleTizen::SetConnectionStateChangedCallback(
    ConnectionStateChangedCallback callback) {
  error_code_ = bt_gatt_set_connection_state_changed_cb(
      [](int result, bool connected, const char *remote_address,
         void *user_data) {
        BleTizen *ble_tizen = static_cast<BleTizen *>(user_data);

        if (ble_tizen->connection_state_changed_callback_) {
          ble_tizen->connection_state_changed_callback_(
              remote_address, connected ? ConnectionState::kConnected
                                        : ConnectionState::kDisconnected);
        }
      },
      this);
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  connection_state_changed_callback_ = callback;
  return true;
}

bool BleTizen::UnsetConnectionStateChangedCallback() {
  error_code_ = bt_gatt_unset_connection_state_changed_cb();
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  connection_state_changed_callback_ = nullptr;
  return true;
}

bool BleTizen::ReadCharacteristics(const CharacteristicAddress &address,
                                   CharacteristicValueReadCallback callback) {
  auto [it, inserted] = native_handles_.try_emplace(address, address);
  if (inserted) {
    LOG_DEBUG("A new characteristic info was inserted: [%s %s %s].",
              address.device_address().c_str(), address.service_uuid().c_str(),
              address.characteristic_uuid().c_str());
  } else {
    LOG_DEBUG("Using exising characteristic info: [%s %s %s].",
              address.device_address().c_str(), address.service_uuid().c_str(),
              address.characteristic_uuid().c_str());
  }
  NativeCharacteristicHandle *native_handle = &it->second;
  if (!native_handle->IsReadable()) {
    LOG_ERROR("The characteristic is not readable.");
    error_code_ = BT_ERROR_NOT_SUPPORTED;
    return false;
  }
  native_handle->SetValueReadCallback(
      [this, callback](CharacteristicAddress address,
                       std::vector<uint8_t> data) {
        callback(address, data);
        if (foward_notifications_callback_) {
          foward_notifications_callback_(address, data);
        }
      });

  // TODO: Handle locks when callbacks are run by different threads.
  error_code_ = bt_gatt_client_read_value(
      native_handle->characteristic_handle(),
      [](int result, bt_gatt_h request_handle, void *user_data) {
        if (result != BT_ERROR_NONE) {
          // TODO: Add error callbacks.
          LOG_ERROR("Read value returns error on callback: %s",
                    get_error_message(result));
          return;
        }
        NativeCharacteristicHandle *native_handle =
            static_cast<NativeCharacteristicHandle *>(user_data);
        char *value;
        int length;
        bt_gatt_get_value(request_handle, &value, &length);
        std::vector<uint8_t> bytes(value, value + length);
        free(value);
        native_handle->NotifyValueRead(bytes);
        native_handle->SetValueReadCallback(nullptr);
      },
      native_handle);
  if (error_code_ != BT_ERROR_NONE) {
    native_handle->SetValueReadCallback(nullptr);
    return false;
  }
  return true;
}

bool BleTizen::WriteCharacteristics(const CharacteristicAddress &address,
                                    const std::vector<uint8_t> &value,
                                    CharacteristicValueWriteCallback callback) {
  auto [it, _] = native_handles_.try_emplace(address, address);
  NativeCharacteristicHandle *native_handle = &it->second;
  error_code_ = bt_gatt_set_value(native_handle->characteristic_handle(),
                                  reinterpret_cast<const char *>(value.data()),
                                  static_cast<int>(value.size()));
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }

  native_handle->SetValueWroteCallback(callback);
  // TODO: Handle locks when callbacks are run by different threads.
  error_code_ = bt_gatt_client_write_value(
      native_handle->characteristic_handle(),
      [](int result, bt_gatt_h request_handle, void *user_data) {
        if (result != BT_ERROR_NONE) {
          // TODO: Add error callbacks.
        }
        NativeCharacteristicHandle *native_handle =
            static_cast<NativeCharacteristicHandle *>(user_data);
        native_handle->NotifyValueWrote();
        native_handle->SetValueWroteCallback(nullptr);
      },
      native_handle);
  if (error_code_ != BT_ERROR_NONE) {
    native_handle->SetValueWroteCallback(nullptr);
    return false;
  }
  return true;
}

bool BleTizen::ListenNotifications(
    const CharacteristicAddress &address,
    CharacteristicValueChangedCallback callback) {
  auto [it, _] = native_handles_.try_emplace(address, address);
  NativeCharacteristicHandle *native_handle = &it->second;
  native_handle->SetValueChangedCallback(
      [this, callback](CharacteristicAddress address,
                       std::vector<uint8_t> data) {
        callback(address, data);
        if (foward_notifications_callback_) {
          foward_notifications_callback_(address, data);
        }
      });

  // TODO: Handle locks when callbacks are run by different threads.
  error_code_ = bt_gatt_client_set_characteristic_value_changed_cb(
      native_handle->characteristic_handle(),
      [](bt_gatt_h characteristic, char *value, int len, void *user_data) {
        NativeCharacteristicHandle *native_handle =
            static_cast<NativeCharacteristicHandle *>(user_data);
        std::vector<uint8_t> bytes(value, value + len);
        native_handle->NotifyValueChanged(bytes);
      },
      native_handle);
  if (error_code_ != BT_ERROR_NONE) {
    native_handle->SetValueChangedCallback(nullptr);
    return false;
  }
  return true;
}

bool BleTizen::StopNotifications(const CharacteristicAddress &address) {
  auto it = native_handles_.find(address);
  if (it == native_handles_.end()) {
    return true;
  }
  NativeCharacteristicHandle *native_handle = &it->second;
  error_code_ = bt_gatt_client_unset_characteristic_value_changed_cb(
      native_handle->characteristic_handle());
  if (error_code_ != BT_ERROR_NONE) {
    return false;
  }
  native_handle->SetValueChangedCallback(nullptr);
  return true;
}

void BleTizen::FowardAllNotifications(
    CharacteristicValueChangedCallback callback) {
  foward_notifications_callback_ = callback;
}

void BleTizen::StopFowardingAllNotifications() {
  foward_notifications_callback_ = nullptr;
}

std::optional<DiscoveredServicesInfo> BleTizen::DiscoverServices(
    const std::string &device_address) {
  DiscoveredServicesInfo discovered_services_info;
  discovered_services_info.device_id = device_address;

  bt_gatt_client_h device_handle;
  error_code_ = bt_gatt_client_create(device_address.c_str(), &device_handle);
  if (error_code_ != BT_ERROR_NONE) {
    return std::nullopt;
  }
  error_code_ = bt_gatt_client_foreach_services(
      device_handle,
      [](int total, int index, bt_gatt_h service_handle, void *user_data) {
        DiscoveredServicesInfo *discovered_services_info =
            static_cast<DiscoveredServicesInfo *>(user_data);
        GetDiscoveredService(service_handle,
                             &discovered_services_info->services);
        return true;
      },
      &discovered_services_info);
  if (error_code_ != BT_ERROR_NONE) {
    return std::nullopt;
  }

  error_code_ = bt_gatt_client_destroy(device_handle);
  if (error_code_ != BT_ERROR_NONE) {
    return std::nullopt;
  }
  return discovered_services_info;
}

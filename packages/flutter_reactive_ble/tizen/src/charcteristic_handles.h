#ifndef FLUTTER_PLUGIN_CHARACTERISTIC_HANDLES_TIZEN_H_
#define FLUTTER_PLUGIN_CHARACTERISTIC_HANDLES_TIZEN_H_

#include <bluetooth.h>
#include <bluetooth_type.h>

#include <functional>
#include <string>

typedef std::string uuid;

// A value type class that locates characteristic.
class CharacteristicAddress {
 public:
  CharacteristicAddress(const std::string &device_address,
                        const uuid &service_uuid,
                        const uuid &characteristic_uuid)
      : device_address_(device_address),
        service_uuid_(service_uuid),
        characteristic_uuid_(characteristic_uuid) {}

  std::string device_address() const { return device_address_; }

  uuid service_uuid() const { return service_uuid_; }

  uuid characteristic_uuid() const { return characteristic_uuid_; }

  bool operator==(const CharacteristicAddress &other) const {
    return (device_address_ == other.device_address_ &&
            service_uuid_ == other.service_uuid_ &&
            characteristic_uuid_ == other.characteristic_uuid_);
  }

 private:
  std::string device_address_;
  uuid service_uuid_;
  uuid characteristic_uuid_;
};

struct CharacteristicAddressHasher {
  std::size_t operator()(const CharacteristicAddress &address) const {
    return ((std::hash<std::string>()(address.device_address()) ^
             (std::hash<std::string>()(address.service_uuid()) << 1)) >>
            1) ^
           (std::hash<std::string>()(address.characteristic_uuid()) << 1);
  }
};

typedef std::function<void(CharacteristicAddress address, std::vector<uint8_t>)>
    CharacteristicValueChangedCallback;
typedef std::function<void(CharacteristicAddress address, std::vector<uint8_t>)>
    CharacteristicValueReadCallback;
typedef std::function<void(CharacteristicAddress address)>
    CharacteristicValueWriteCallback;

/// A wrapper class that holds native ble device and GATT handles.
class NativeCharacteristicHandle {
 public:
  /// Creates a NativeCharacteristicHandle instance from CharacteristicAddress.
  explicit NativeCharacteristicHandle(const CharacteristicAddress &address) {
    bt_gatt_client_create(address.device_address().c_str(), &device_handle_);
    bt_gatt_client_get_service(device_handle_, address.service_uuid().c_str(),
                               &service_handle_);
    bt_gatt_service_get_characteristic(service_handle_,
                                       address.characteristic_uuid().c_str(),
                                       &characteristic_handle_);
  }

  // Delete copy operations.
  NativeCharacteristicHandle(const NativeCharacteristicHandle &handle) = delete;
  NativeCharacteristicHandle &operator=(
      const NativeCharacteristicHandle &handle) = delete;

  // Define move constructor.
  NativeCharacteristicHandle(NativeCharacteristicHandle &&handle)
      : device_handle_(std::move(handle.device_handle_)),
        service_handle_(std::move(handle.service_handle_)),
        characteristic_handle_(std::move(handle.characteristic_handle_)),
        characteristic_value_changed_callback_(
            std::move(handle.characteristic_value_changed_callback_)),
        characteristic_value_read_callback_(
            std::move(handle.characteristic_value_read_callback_)),
        characteristic_value_write_callback_(
            std::move(handle.characteristic_value_write_callback_)){};
  // Delete move assignment.
  NativeCharacteristicHandle &operator=(NativeCharacteristicHandle &&handle) =
      delete;

  ~NativeCharacteristicHandle() {
    if (!device_handle_) {
      bt_gatt_client_destroy(device_handle_);
    }
  }

  /// Returns a CharacteristicAddress from the current native handle.
  CharacteristicAddress ToCharacteristicAddress() const {
    return CharacteristicAddress(device_id(), service_uuid(),
                                 characteristic_uuid());
  }

  std::string device_id() const { return GetDeviceAddress(device_handle_); }

  uuid service_uuid() const { return GetUuid(service_handle_); }

  uuid characteristic_uuid() const { return GetUuid(characteristic_handle_); }

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
      characteristic_value_changed_callback_(ToCharacteristicAddress(), value);
    }
  }

  void NotifyValueRead(const std::vector<uint8_t> &value) const {
    if (characteristic_value_read_callback_) {
      characteristic_value_read_callback_(ToCharacteristicAddress(), value);
    }
  }

  void NotifyValueWrote() const {
    if (characteristic_value_write_callback_) {
      characteristic_value_write_callback_(ToCharacteristicAddress());
    }
  }

  bool IsReadable() const {
    int properties;
    bt_gatt_characteristic_get_properties(characteristic_handle_, &properties);
    return properties & BT_GATT_PROPERTY_READ;
  }

 private:
  /// Device handle.
  bt_gatt_client_h device_handle_;

  /// Service GATT handle.
  bt_gatt_h service_handle_;

  /// Characteristic GATT handle.
  bt_gatt_h characteristic_handle_;

  CharacteristicValueChangedCallback characteristic_value_changed_callback_;

  CharacteristicValueReadCallback characteristic_value_read_callback_;

  CharacteristicValueWriteCallback characteristic_value_write_callback_;

  /// Gets the uuid of GATT handle.
  static uuid GetUuid(const bt_gatt_h &gatt_handle) {
    char *uuid_cstr;
    bt_gatt_get_uuid(gatt_handle, &uuid_cstr);
    std::string uuid = std::string(uuid_cstr);
    free(uuid_cstr);
    return uuid;
  }

  /// Gets the address of device.
  static std::string GetDeviceAddress(const bt_gatt_client_h &device_handle) {
    char *device_address_cstr;
    bt_gatt_client_get_remote_address(device_handle, &device_address_cstr);
    std::string device_address = std::string(device_address_cstr);
    free(device_address_cstr);
    return device_address;
  }
};

#endif  // FLUTTER_PLUGIN_CHARACTERISTIC_HANDLES_TIZEN_H_
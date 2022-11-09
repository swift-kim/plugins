#ifndef FLUTTER_PLUGIN_BLE_TIZEN_H_
#define FLUTTER_PLUGIN_BLE_TIZEN_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "charcteristic_handles.h"

struct ServiceDataEntry {
  uuid service_id;
  std::vector<std::uint8_t> data;
};

struct DeviceScanInfo {
  std::string address;

  std::string name;

  std::vector<ServiceDataEntry> service_data;

  std::vector<uint8_t> manufacturer_data;

  std::vector<uuid> service_uuids;

  int32_t rssi;
};

struct ServiceWithCharacteristics {
  uuid service_id;
  std::vector<uuid> characteristics;
};

struct DiscoveredCharacteristic {
  uuid characteristic_id;
  uuid service_id;
  bool is_readable;
  bool is_writable_with_response;
  bool is_writable_without_response;
  bool is_notifiable;
  bool is_indicatable;
};

struct DiscoveredService {
  uuid service_id;
  std::vector<uuid> characteristic_uuids;
  std::vector<DiscoveredService> included_services;
  std::vector<DiscoveredCharacteristic> characteristics;
};

struct DiscoveredServicesInfo {
  std::string device_id;
  std::vector<DiscoveredService> services;
};

class ErrorReportMixin {
 public:
  int error_code() const { return error_code_; }

  std::string error_message() const;

 protected:
  int error_code_;
};

// A class that holds and applys filtering information for scanning.
class Filter : public ErrorReportMixin {
 public:
  virtual ~Filter() = default;

  virtual bool Register() = 0;

  virtual bool Unregister() = 0;

  virtual const bt_scan_filter_h& handle() const = 0;
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
typedef std::function<void(std::string device_address, ConnectionState)>
    ConnectionStateChangedCallback;

class BleTizen : public ErrorReportMixin {
 public:
  ~BleTizen();

  // Initializes the bluetooth low enery module.
  bool Initialize();

  // Clean ups the bluetooth low enery module.
  bool Deinitialize();

  std::optional<BleStatus> GetBleStatus();

  bool SetBleStatusChangedCallback(BleStatusChangedCallback callback);

  bool UnsetBleStatusChangedCallback();

  // Scans for surrounding devices.
  bool Scan(const std::vector<uuid>& service_filters);

  // Stops scanning for devices.
  bool StopScan();

  void SetDeviceScanCallback(DeviceScanCallback callback);

  void UnsetDeviceScanCallback();

  // Connects to device with |device_address|.
  bool ConnectToDevice(const std::string& device_address);

  // Disconnects from device with |device_address|.
  bool DisconnectFromDevice(const std::string& device_address);

  bool SetConnectionStateChangedCallback(
      ConnectionStateChangedCallback callback);

  bool UnsetConnectionStateChangedCallback();

  // Reads value from characteristic.
  bool ReadCharacteristics(const CharacteristicAddress& address,
                           CharacteristicValueReadCallback callback);

  // Writes value to characteristic.
  bool WriteCharacteristics(const CharacteristicAddress& address,
                            const std::vector<uint8_t>& value,
                            CharacteristicValueWriteCallback callback);

  // Listens for notification from characteristic.
  bool ListenNotifications(const CharacteristicAddress& address,
                           CharacteristicValueChangedCallback callback);

  // Stops listening for notifications from characteristic.
  bool StopNotifications(const CharacteristicAddress& address);

  void FowardAllNotifications(CharacteristicValueChangedCallback callback);

  void StopFowardingAllNotifications();

  // Discovers services adverstised by the device.
  std::optional<DiscoveredServicesInfo> DiscoverServices(
      const std::string& device_address);

 private:
  bool scanning_ = false;

  BleStatusChangedCallback ble_status_changed_callback_;
  DeviceScanCallback device_scan_callback_;
  ConnectionStateChangedCallback connection_state_changed_callback_;

  std::unordered_map<CharacteristicAddress, NativeCharacteristicHandle,
                     CharacteristicAddressHasher>
      native_handles_;

  CharacteristicValueChangedCallback foward_notifications_callback_;

  std::vector<std::unique_ptr<Filter>> filters_;
};

#endif  // FLUTTER_PLUGIN_BLE_TIZEN_H_
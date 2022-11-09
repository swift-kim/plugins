#include "flutter_reactive_ble_tizen_plugin.h"

#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>
#include <system_info.h>

#include <memory>
#include <sstream>
#include <string>

#include "ble_tizen.h"
#include "log.h"

namespace {

const char *kInvalidArgument = "Invalid argument";

// String key constants for methodcall arguments and results.
// Keep this in sync with those in `models.dart`.
const char *kBleStatus = "ble_status";
const char *kCharacteristicId = "characteristic_id";
const char *kCharacteristicIds = "characteristic_ids";
const char *kCharacteristics = "characteristics";
const char *kConnectionState = "connection_state";
const char *kDeviceId = "device_id";
const char *kDeviceName = "device_name";
const char *kDiscoveredDevice = "discovered_device";
const char *kFailure = "failure";
const char *kIncludedServices = "included_services";
const char *kIsIndicatable = "is_indicatable";
const char *kIsNotifiable = "is_notifiable";
const char *kIsReadable = "is_readable";
const char *kIsWritableWithoutResponse = "is_writable_without_response";
const char *kIsWritableWithResponse = "is_writable_with_response";
const char *kManufacturerData = "manufacturer_data";
const char *kMessage = "message";
const char *kQualifiedCharacteristic = "qualified_characteristic";
const char *kResult = "result";
const char *kRssi = "rssi";
const char *kServiceData = "service_data";
const char *kServiceId = "service_id";
const char *kServiceIds = "service_ids";
const char *kServices = "services";
const char *kValue = "value";

std::string GetArgMissingOrTypeErrorMessage(const char *argument_name) {
  std::stringstream string_stream;
  string_stream << "Argument " << argument_name
                << " not provided, or wrong expected type.";
  return string_stream.str();
}

template <typename T>
bool GetValueFromEncodableMap(const flutter::EncodableMap *map, const char *key,
                              T &out) {
  auto iter = map->find(flutter::EncodableValue(key));
  if (iter != map->end() && !iter->second.IsNull()) {
    if (auto *value = std::get_if<T>(&iter->second)) {
      out = *value;
      return true;
    }
  }
  return false;
}

// Helper method to pack DiscoveredService into EncodableValue recursively.
flutter::EncodableValue PackToEncodableValue(
    const DiscoveredService &discovered_service) {
  flutter::EncodableList characteristic_uuids;
  std::transform(discovered_service.characteristic_uuids.begin(),
                 discovered_service.characteristic_uuids.end(),
                 std::back_inserter(characteristic_uuids),
                 [](const uuid &characteristic_uuid) {
                   return flutter::EncodableValue(characteristic_uuid);
                 });

  flutter::EncodableList included_services;
  std::transform(discovered_service.included_services.begin(),
                 discovered_service.included_services.end(),
                 std::back_inserter(included_services),
                 [](const DiscoveredService &discovered_service) {
                   return PackToEncodableValue(discovered_service);
                 });

  flutter::EncodableList characteristics;
  std::transform(
      discovered_service.characteristics.begin(),
      discovered_service.characteristics.end(),
      std::back_inserter(characteristics),
      [](const DiscoveredCharacteristic &discovered_characteristic) {
        return flutter::EncodableValue(flutter::EncodableMap({
            {flutter::EncodableValue(kCharacteristicId),
             flutter::EncodableValue(
                 discovered_characteristic.characteristic_id)},
            {flutter::EncodableValue(kServiceId),
             flutter::EncodableValue(discovered_characteristic.service_id)},
            {flutter::EncodableValue(kIsReadable),
             flutter::EncodableValue(discovered_characteristic.is_readable)},
            {flutter::EncodableValue(kIsWritableWithResponse),
             flutter::EncodableValue(
                 discovered_characteristic.is_writable_with_response)},
            {flutter::EncodableValue(kIsWritableWithoutResponse),
             flutter::EncodableValue(
                 discovered_characteristic.is_writable_without_response)},
            {flutter::EncodableValue(kIsNotifiable),
             flutter::EncodableValue(discovered_characteristic.is_notifiable)},
            {flutter::EncodableValue(kIsIndicatable),
             flutter::EncodableValue(discovered_characteristic.is_indicatable)},
        }));
      });

  flutter::EncodableMap encodable_map = {
      {flutter::EncodableValue(kServiceId),
       flutter::EncodableValue(discovered_service.service_id)},
      {flutter::EncodableValue(kCharacteristicIds),
       flutter::EncodableValue(characteristic_uuids)},
      {flutter::EncodableValue(kIncludedServices),
       flutter::EncodableValue(included_services)},
      {flutter::EncodableValue(kCharacteristics),
       flutter::EncodableValue(characteristics)},
  };
  return flutter::EncodableValue(encodable_map);
}

typedef flutter::EventChannel<flutter::EncodableValue> FlEventChannel;
typedef flutter::EventSink<flutter::EncodableValue> FlEventSink;
typedef flutter::StreamHandler<flutter::EncodableValue> FlStreamHandler;
typedef flutter::StreamHandlerError<flutter::EncodableValue>
    FlStreamHandlerError;

class BleCharUpdateStreamHandler : public FlStreamHandler {
 public:
  explicit BleCharUpdateStreamHandler(BleTizen *ble_tizen)
      : ble_tizen_(ble_tizen){};

 protected:
  std::unique_ptr<FlStreamHandlerError> OnListenInternal(
      const flutter::EncodableValue *arguments,
      std::unique_ptr<FlEventSink> &&events) override {
    events_ = std::move(events);
    ble_tizen_->FowardAllNotifications(
        [this](CharacteristicAddress address, std::vector<uint8_t> data) {
          flutter::EncodableMap qualified_characteristic_map = {
              {flutter::EncodableValue(kDeviceId),
               flutter::EncodableValue(address.device_address())},
              {flutter::EncodableValue(kServiceId),
               flutter::EncodableValue(address.service_uuid())},
              {flutter::EncodableValue(kCharacteristicId),
               flutter::EncodableValue(address.characteristic_uuid())},
          };
          flutter::EncodableMap result_map = {
              {flutter::EncodableValue(kValue), flutter::EncodableValue(data)},
          };
          events_->Success(flutter::EncodableValue(flutter::EncodableMap({
              {flutter::EncodableValue(kQualifiedCharacteristic),
               flutter::EncodableValue(qualified_characteristic_map)},
              {flutter::EncodableValue(kResult),
               flutter::EncodableValue(result_map)},
          })));
        });
    return nullptr;
  }

  std::unique_ptr<FlStreamHandlerError> OnCancelInternal(
      const flutter::EncodableValue *arguments) override {
    ble_tizen_->StopFowardingAllNotifications();
    events_.reset();
    return nullptr;
  }

 private:
  BleTizen *ble_tizen_;
  std::unique_ptr<FlEventSink> events_;
};

class ConnectionUpdateStreamHandler : public FlStreamHandler {
 public:
  explicit ConnectionUpdateStreamHandler(BleTizen *ble_tizen)
      : ble_tizen_(ble_tizen){};

 protected:
  std::unique_ptr<FlStreamHandlerError> OnListenInternal(
      const flutter::EncodableValue *arguments,
      std::unique_ptr<FlEventSink> &&events) override {
    events_ = std::move(events);
    ble_tizen_->SetConnectionStateChangedCallback(
        [this](const std::string &device_address,
               ConnectionState connection_state) {
          std::string connection_state_str;
          switch (connection_state) {
            case ConnectionState::kConnecting:
              connection_state_str = "connecting";
              break;
            case ConnectionState::kConnected:
              connection_state_str = "connected";
              break;
            case ConnectionState::kDisconnecting:
              connection_state_str = "disconnecting";
              break;
            case ConnectionState::kDisconnected:
              connection_state_str = "disconnected";
              break;
            default:
              assert(false);
              break;
          }
          flutter::EncodableMap encodable_map = {
              {flutter::EncodableValue(kDeviceId),
               flutter::EncodableValue(device_address)},
              {flutter::EncodableValue(kConnectionState),
               flutter::EncodableValue(connection_state_str)},
          };
          events_->Success(flutter::EncodableValue(encodable_map));
        });
    return nullptr;
  }

  std::unique_ptr<FlStreamHandlerError> OnCancelInternal(
      const flutter::EncodableValue *arguments) override {
    ble_tizen_->UnsetConnectionStateChangedCallback();
    events_.reset();
    return nullptr;
  }

 private:
  BleTizen *ble_tizen_;
  std::unique_ptr<FlEventSink> events_;
};

class BleScanStreamHandler : public FlStreamHandler {
 public:
  explicit BleScanStreamHandler(BleTizen *ble_tizen) : ble_tizen_(ble_tizen){};

 protected:
  std::unique_ptr<FlStreamHandlerError> OnListenInternal(
      const flutter::EncodableValue *arguments,
      std::unique_ptr<FlEventSink> &&events) override {
    events_ = std::move(events);
    ble_tizen_->SetDeviceScanCallback(
        [this](std::unique_ptr<DeviceScanInfo> device_scan_info) {
          flutter::EncodableMap service_data;
          std::transform(device_scan_info->service_data.begin(),
                         device_scan_info->service_data.end(),
                         std::inserter(service_data, service_data.begin()),
                         [](const ServiceDataEntry &element) {
                           return std::make_pair(
                               flutter::EncodableValue(element.service_id),
                               flutter::EncodableValue(element.data));
                         });

          flutter::EncodableList service_uuids;
          std::transform(device_scan_info->service_uuids.begin(),
                         device_scan_info->service_uuids.end(),
                         std::back_inserter(service_uuids),
                         [](const uuid &service_uuid) {
                           return flutter::EncodableValue(service_uuid);
                         });

          flutter::EncodableMap encodable_map = {
              {
                  flutter::EncodableValue(kDiscoveredDevice),
                  flutter::EncodableValue(flutter::EncodableMap({
                      {flutter::EncodableValue(kDeviceId),
                       flutter::EncodableValue(device_scan_info->address)},
                      {flutter::EncodableValue(kDeviceName),
                       flutter::EncodableValue(device_scan_info->name)},
                      {flutter::EncodableValue(kServiceData),
                       flutter::EncodableValue(service_data)},
                      {flutter::EncodableValue(kManufacturerData),
                       flutter::EncodableValue(
                           device_scan_info->manufacturer_data)},
                      {flutter::EncodableValue(kRssi),
                       flutter::EncodableValue(device_scan_info->rssi)},
                      {flutter::EncodableValue(kServiceIds),
                       flutter::EncodableValue(service_uuids)},
                  })),
              },
          };
          events_->Success(flutter::EncodableValue(encodable_map));
        });
    return nullptr;
  }

  std::unique_ptr<FlStreamHandlerError> OnCancelInternal(
      const flutter::EncodableValue *arguments) override {
    ble_tizen_->UnsetDeviceScanCallback();
    ble_tizen_->StopScan();
    events_.reset();
    return nullptr;
  }

 private:
  BleTizen *ble_tizen_;
  std::unique_ptr<FlEventSink> events_;
};

const char *kUnsupported = "unsupported";
const char *kPoweredOff = "poweredOff";
const char *kReady = "ready";

class BleStatusStreamHandler : public FlStreamHandler {
 public:
  explicit BleStatusStreamHandler(BleTizen *ble_tizen)
      : ble_tizen_(ble_tizen){};

 protected:
  std::unique_ptr<FlStreamHandlerError> OnListenInternal(
      const flutter::EncodableValue *arguments,
      std::unique_ptr<FlEventSink> &&events) override {
    // Dart side accepts one of the following states in string type.
    //  - unsupported: BLE is not supported on this device.
    //  - poweredOff: BLE is turned off.
    //  - ready: BLE is fully operating for this app.
    events_ = std::move(events);
    auto ble_status = ble_tizen_->GetBleStatus();
    std::string ble_status_str;
    if (ble_status.has_value()) {
      switch (ble_status.value()) {
        case BleStatus::kEnabled:
          ble_status_str = kReady;
          break;
        case BleStatus::kDisabled:
          ble_status_str = kPoweredOff;
          break;
        case BleStatus::kNotSupported:
          ble_status_str = kUnsupported;
          break;
        default:
          assert(false);
          break;
      }
    }
    flutter::EncodableMap encodable_map = {
        {flutter::EncodableValue(kBleStatus),
         flutter::EncodableValue(ble_status_str)},
    };
    events_->Success(flutter::EncodableValue(encodable_map));

    ble_tizen_->SetBleStatusChangedCallback([this](BleStatus ble_status) {
      std::string ble_status_str =
          ble_status == BleStatus::kEnabled ? kReady : kPoweredOff;
      flutter::EncodableMap encodable_map = {
          {flutter::EncodableValue(kBleStatus),
           flutter::EncodableValue(ble_status_str)},
      };
      events_->Success(flutter::EncodableValue(encodable_map));
    });
    return nullptr;
  }

  std::unique_ptr<FlStreamHandlerError> OnCancelInternal(
      const flutter::EncodableValue *arguments) override {
    ble_tizen_->UnsetBleStatusChangedCallback();
    events_.reset();
    return nullptr;
  }

 private:
  BleTizen *ble_tizen_;
  std::unique_ptr<FlEventSink> events_;
};

class FlutterReactiveBleTizenPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar) {
    auto channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "flutter_reactive_ble_method",
            &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<FlutterReactiveBleTizenPlugin>();
    plugin->ble_tizen_ = std::make_unique<BleTizen>();

    plugin->SetupEventChannels(registrar);

    channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto &call, auto result) {
          plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
  }

  FlutterReactiveBleTizenPlugin() {}

  virtual ~FlutterReactiveBleTizenPlugin() {}

 private:
  // Setup event channels.
  void SetupEventChannels(flutter::PluginRegistrar *registrar) {
    connected_device_channel_ = std::make_unique<FlEventChannel>(
        registrar->messenger(), "flutter_reactive_ble_connected_device",
        &flutter::StandardMethodCodec::GetInstance());
    connected_device_channel_->SetStreamHandler(
        std::make_unique<ConnectionUpdateStreamHandler>(ble_tizen_.get()));

    char_event_channel_ = std::make_unique<FlEventChannel>(
        registrar->messenger(), "flutter_reactive_ble_char_update",
        &flutter::StandardMethodCodec::GetInstance());
    char_event_channel_->SetStreamHandler(
        std::make_unique<BleCharUpdateStreamHandler>(ble_tizen_.get()));

    scan_event_channel_ = std::make_unique<FlEventChannel>(
        registrar->messenger(), "flutter_reactive_ble_scan",
        &flutter::StandardMethodCodec::GetInstance());
    scan_event_channel_->SetStreamHandler(
        std::make_unique<BleScanStreamHandler>(ble_tizen_.get()));

    ble_status_channel_ = std::make_unique<FlEventChannel>(
        registrar->messenger(), "flutter_reactive_ble_status",
        &flutter::StandardMethodCodec::GetInstance());
    ble_status_channel_->SetStreamHandler(
        std::make_unique<BleStatusStreamHandler>(ble_tizen_.get()));
  }

  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    const auto &method_name = method_call.method_name();
    if (method_name == "initialize") {
      if (!ble_tizen_->Initialize()) {
        result->Error("Initialize failed.", ble_tizen_->error_message());
        return;
      };
      result->Success();
    } else if (method_name == "deinitialize") {
      if (!ble_tizen_->Deinitialize()) {
        result->Error("Deinitialize failed", ble_tizen_->error_message());
        return;
      };
      result->Success();
    } else if (method_name == "scanForDevices") {
      const auto *arguments =
          std::get_if<flutter::EncodableMap>(method_call.arguments());
      if (!arguments) {
        result->Error(kInvalidArgument, "No arguments provided.");
        return;
      }
      flutter::EncodableList encodable_list;
      if (!GetValueFromEncodableMap(arguments, kServiceIds, encodable_list)) {
        result->Error(kInvalidArgument,
                      GetArgMissingOrTypeErrorMessage(kServiceIds));
        return;
      }
      std::vector<uuid> service_ids;
      for (const auto &encodable : encodable_list) {
        const std::string *service_id = std::get_if<std::string>(&encodable);
        if (service_id == nullptr) {
          result->Error(kInvalidArgument,
                        "Service Ids are not of type string.");
          return;
        }
        service_ids.push_back(*service_id);
      }

      if (!ble_tizen_->Scan(service_ids)) {
        result->Error("Device scanning failed.", ble_tizen_->error_message());
        return;
      };
      result->Success();
    } else if (method_name == "connectToDevice") {
      auto device_address = GetDeviceAddressArgument(method_call, result.get());
      if (!device_address.has_value()) {
        return;
      }
      if (!ble_tizen_->ConnectToDevice(device_address.value())) {
        result->Error("Connecting to device failed.",
                      ble_tizen_->error_message());
        return;
      };
      result->Success();
    } else if (method_name == "disconnectDevice") {
      auto device_address = GetDeviceAddressArgument(method_call, result.get());
      if (!device_address.has_value()) {
        return;
      }
      if (!ble_tizen_->DisconnectFromDevice(device_address.value())) {
        result->Error("Disconnecting to device failed.",
                      ble_tizen_->error_message());
        return;
      };
      result->Success();
    } else if (method_name == "readCharacteristic") {
      auto characteristic_address =
          GetQualifiedCharacteristicArgument(method_call, result.get());
      if (!characteristic_address.has_value()) {
        return;
      }
      if (!ble_tizen_->ReadCharacteristics(characteristic_address.value(),
                                           [](CharacteristicAddress address,
                                              std::vector<uint8_t> value) {})) {
        result->Error("Reading characteristic failed.",
                      ble_tizen_->error_message());
        return;
      }
      result->Success();
    } else if (method_name == "writeCharacteristicWithResponse") {
      result->NotImplemented();
    } else if (method_name == "writeCharacteristicWithoutResponse") {
      result->NotImplemented();
    } else if (method_name == "subscribeToNotifications") {
      auto characteristic_address =
          GetQualifiedCharacteristicArgument(method_call, result.get());
      if (!characteristic_address.has_value()) {
        return;
      }
      if (!ble_tizen_->ListenNotifications(characteristic_address.value(),
                                           [](CharacteristicAddress address,
                                              std::vector<uint8_t> value) {})) {
        result->Error("Subscribing to notifications failed.",
                      ble_tizen_->error_message());
        return;
      }
      result->Success();
    } else if (method_name == "Stop subscribing to notifications failed.") {
      auto characteristic_address =
          GetQualifiedCharacteristicArgument(method_call, result.get());
      if (!characteristic_address.has_value()) {
        return;
      }
      if (!ble_tizen_->StopNotifications(characteristic_address.value())) {
        result->Error("stopSubscribingToNotifications",
                      ble_tizen_->error_message());
        return;
      }
      result->Success();
    } else if (method_name == "discoverServices") {
      auto device_address = GetDeviceAddressArgument(method_call, result.get());
      if (!device_address.has_value()) {
        return;
      }
      auto discovered_services_info =
          ble_tizen_->DiscoverServices(device_address.value());
      if (!discovered_services_info.has_value()) {
        result->Error("Discovering services failed.",
                      ble_tizen_->error_message());
        return;
      }

      flutter::EncodableList discovered_services;
      std::transform(discovered_services_info.value().services.begin(),
                     discovered_services_info.value().services.end(),
                     std::back_inserter(discovered_services),
                     [](const DiscoveredService &discovered_service) {
                       return PackToEncodableValue(discovered_service);
                     });

      flutter::EncodableMap encodable_map = {
          {flutter::EncodableValue(kDeviceId),
           flutter::EncodableValue(discovered_services_info.value().device_id)},
          {flutter::EncodableValue(kServices),
           flutter::EncodableValue(discovered_services)},
      };
      result->Success(flutter::EncodableValue(encodable_map));
    } else {
      result->NotImplemented();
    }
  }

  // Helper method to get device id argument from MethodCall and handle
  // errors if not present.
  std::optional<std::string> GetDeviceAddressArgument(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      flutter::MethodResult<flutter::EncodableValue> *result) {
    const auto *arguments =
        std::get_if<flutter::EncodableMap>(method_call.arguments());
    if (!arguments) {
      result->Error(kInvalidArgument, "No arguments provided.");
      return std::nullopt;
    }
    std::string device_address;
    if (!GetValueFromEncodableMap(arguments, kDeviceId, device_address)) {
      result->Error(kInvalidArgument,
                    GetArgMissingOrTypeErrorMessage(kDeviceId));
      return std::nullopt;
    }
    return device_address;
  }

  // Helper method to get qualified characteristic argument from MethodCall and
  // handle errors if not present.
  std::optional<CharacteristicAddress> GetQualifiedCharacteristicArgument(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      flutter::MethodResult<flutter::EncodableValue> *result) {
    const auto *arguments =
        std::get_if<flutter::EncodableMap>(method_call.arguments());
    if (!arguments) {
      result->Error(kInvalidArgument, "No arguments provided.");
      return std::nullopt;
    }
    flutter::EncodableMap qualified_characteristic;
    if (!GetValueFromEncodableMap(arguments, kQualifiedCharacteristic,
                                  qualified_characteristic)) {
      result->Error(kInvalidArgument,
                    GetArgMissingOrTypeErrorMessage(kQualifiedCharacteristic));
      return std::nullopt;
    }
    std::string device_address;
    if (!GetValueFromEncodableMap(&qualified_characteristic, kDeviceId,
                                  device_address)) {
      result->Error(kInvalidArgument,
                    GetArgMissingOrTypeErrorMessage(kDeviceId));
      return std::nullopt;
    }
    std::string service_uuid;
    if (!GetValueFromEncodableMap(&qualified_characteristic, kServiceId,
                                  service_uuid)) {
      result->Error(kInvalidArgument,
                    GetArgMissingOrTypeErrorMessage(kServiceId));
      return std::nullopt;
    }
    std::string characteristic_uuid;
    if (!GetValueFromEncodableMap(&qualified_characteristic, kCharacteristicId,
                                  characteristic_uuid)) {
      result->Error(kInvalidArgument,
                    GetArgMissingOrTypeErrorMessage(kCharacteristicId));
      return std::nullopt;
    }
    return CharacteristicAddress(device_address, service_uuid,
                                 characteristic_uuid);
  }

  std::unique_ptr<FlEventChannel> connected_device_channel_;
  std::unique_ptr<FlEventChannel> char_event_channel_;
  std::unique_ptr<FlEventChannel> scan_event_channel_;
  std::unique_ptr<FlEventChannel> ble_status_channel_;

  std::unique_ptr<BleTizen> ble_tizen_;
};

}  // namespace

void FlutterReactiveBleTizenPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  FlutterReactiveBleTizenPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrar>(registrar));
}

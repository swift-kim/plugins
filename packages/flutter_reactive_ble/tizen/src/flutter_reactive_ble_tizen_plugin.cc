// Copyright 2023 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter_reactive_ble_tizen_plugin.h"

#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ble_tizen.h"
#include "log.h"

namespace {

typedef flutter::EventChannel<flutter::EncodableValue> FlEventChannel;
typedef flutter::EventSink<flutter::EncodableValue> FlEventSink;
typedef flutter::MethodCall<flutter::EncodableValue> FlMethodCall;
typedef flutter::MethodResult<flutter::EncodableValue> FlMethodResult;
typedef flutter::MethodChannel<flutter::EncodableValue> FlMethodChannel;
typedef flutter::StreamHandler<flutter::EncodableValue> FlStreamHandler;
typedef flutter::StreamHandlerError<flutter::EncodableValue>
    FlStreamHandlerError;

// String key constants for method call arguments and results.
// Keep these in sync with those in `models.dart`.
constexpr char kCharacteristicId[] = "characteristic_id";
constexpr char kCharacteristicIds[] = "characteristic_ids";
constexpr char kCharacteristics[] = "characteristics";
constexpr char kConnectionState[] = "connection_state";
constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceName[] = "device_name";
constexpr char kIncludedServices[] = "included_services";
constexpr char kIsIndicatable[] = "is_indicatable";
constexpr char kIsNotifiable[] = "is_notifiable";
constexpr char kIsReadable[] = "is_readable";
constexpr char kIsWritableWithoutResponse[] = "is_writable_without_response";
constexpr char kIsWritableWithResponse[] = "is_writable_with_response";
constexpr char kManufacturerData[] = "manufacturer_data";
constexpr char kQualifiedCharacteristic[] = "qualified_characteristic";
constexpr char kResult[] = "result";
constexpr char kRssi[] = "rssi";
constexpr char kServiceData[] = "service_data";
constexpr char kServiceId[] = "service_id";
constexpr char kServiceIds[] = "service_ids";
constexpr char kValue[] = "value";
constexpr char kFailure[] = "failure";

std::string MissingArgumentError(const char *name) {
  std::stringstream stream;
  stream << "Argument " << name << " not provided, or not of expected type.";
  return stream.str();
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

// Packs a DiscoveredService into an EncodableValue recursively.
flutter::EncodableValue ServiceToEncodableValue(
    const DiscoveredService &service) {
  flutter::EncodableList characteristic_ids;
  for (const Uuid &characteristic_id : service.characteristic_ids) {
    characteristic_ids.push_back(flutter::EncodableValue(characteristic_id));
  }
  flutter::EncodableList included_services;
  for (const DiscoveredService &included_service : service.included_services) {
    included_services.push_back(ServiceToEncodableValue(included_service));
  }
  flutter::EncodableList characteristics;
  for (const DiscoveredCharacteristic &characteristic :
       service.characteristics) {
    flutter::EncodableMap map = {
        {flutter::EncodableValue(kCharacteristicId),
         flutter::EncodableValue(characteristic.characteristic_id)},
        {flutter::EncodableValue(kServiceId),
         flutter::EncodableValue(characteristic.service_id)},
        {flutter::EncodableValue(kIsReadable),
         flutter::EncodableValue(characteristic.is_readable)},
        {flutter::EncodableValue(kIsWritableWithResponse),
         flutter::EncodableValue(characteristic.is_writable_with_response)},
        {flutter::EncodableValue(kIsWritableWithoutResponse),
         flutter::EncodableValue(characteristic.is_writable_without_response)},
        {flutter::EncodableValue(kIsNotifiable),
         flutter::EncodableValue(characteristic.is_notifiable)},
        {flutter::EncodableValue(kIsIndicatable),
         flutter::EncodableValue(characteristic.is_indicatable)},
    };
    characteristics.push_back(flutter::EncodableValue(map));
  }
  flutter::EncodableMap map = {
      {flutter::EncodableValue(kServiceId),
       flutter::EncodableValue(service.service_id)},
      {flutter::EncodableValue(kCharacteristicIds),
       flutter::EncodableValue(characteristic_ids)},
      {flutter::EncodableValue(kIncludedServices),
       flutter::EncodableValue(included_services)},
      {flutter::EncodableValue(kCharacteristics),
       flutter::EncodableValue(characteristics)},
  };
  return flutter::EncodableValue(map);
}

class BleCharUpdateStreamHandler : public FlStreamHandler {
 public:
  explicit BleCharUpdateStreamHandler(BleTizen *ble_tizen)
      : ble_tizen_(ble_tizen){};

 protected:
  std::unique_ptr<FlStreamHandlerError> OnListenInternal(
      const flutter::EncodableValue *arguments,
      std::unique_ptr<FlEventSink> &&events) override {
    events_ = std::move(events);
    ble_tizen_->SetNotificationCallback(
        [this](QualifiedCharacteristic characteristic,
               const std::vector<uint8_t> &data) {
          flutter::EncodableMap characteristic_map = {
              {flutter::EncodableValue(kDeviceId),
               flutter::EncodableValue(characteristic.device_id())},
              {flutter::EncodableValue(kServiceId),
               flutter::EncodableValue(characteristic.service_id())},
              {flutter::EncodableValue(kCharacteristicId),
               flutter::EncodableValue(characteristic.characteristic_id())},
          };
          flutter::EncodableMap map = {
              {flutter::EncodableValue(kQualifiedCharacteristic),
               flutter::EncodableValue(characteristic_map)},
              {flutter::EncodableValue(kResult),
               flutter::EncodableValue(flutter::EncodableValue(data))},
          };
          events_->Success(flutter::EncodableValue(map));
        });
    return nullptr;
  }

  std::unique_ptr<FlStreamHandlerError> OnCancelInternal(
      const flutter::EncodableValue *arguments) override {
    ble_tizen_->SetNotificationCallback(nullptr);
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
        [this](const std::string &device_id, ConnectionState connection_state) {
          std::string state = "disconnected";
          if (connection_state == ConnectionState::kConnecting) {
            state = "connecting";
          } else if (connection_state == ConnectionState::kConnected) {
            state = "connected";
          } else if (connection_state == ConnectionState::kDisconnecting) {
            state = "disconnecting";
          }
          flutter::EncodableMap map = {
              {flutter::EncodableValue(kDeviceId),
               flutter::EncodableValue(device_id)},
              {flutter::EncodableValue(kConnectionState),
               flutter::EncodableValue(state)},
          };
          events_->Success(flutter::EncodableValue(map));
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
          for (const ServiceDataEntry &entry : device_scan_info->service_data) {
            service_data[flutter::EncodableValue(entry.service_id)] =
                flutter::EncodableValue(entry.data);
          }
          flutter::EncodableList service_ids;
          for (const Uuid &service_id : device_scan_info->service_ids) {
            service_ids.push_back(flutter::EncodableValue(service_id));
          }
          flutter::EncodableMap discovered_device = {
              {flutter::EncodableValue(kDeviceId),
               flutter::EncodableValue(device_scan_info->device_id)},
              {flutter::EncodableValue(kDeviceName),
               flutter::EncodableValue(device_scan_info->name)},
              {flutter::EncodableValue(kServiceData),
               flutter::EncodableValue(service_data)},
              {flutter::EncodableValue(kManufacturerData),
               flutter::EncodableValue(device_scan_info->manufacturer_data)},
              {flutter::EncodableValue(kRssi),
               flutter::EncodableValue(device_scan_info->rssi)},
              {flutter::EncodableValue(kServiceIds),
               flutter::EncodableValue(service_ids)},
          };
          events_->Success(flutter::EncodableValue(discovered_device));
        });
    return nullptr;
  }

  std::unique_ptr<FlStreamHandlerError> OnCancelInternal(
      const flutter::EncodableValue *arguments) override {
    ble_tizen_->SetDeviceScanCallback(nullptr);
    ble_tizen_->StopScan();
    events_.reset();
    return nullptr;
  }

 private:
  BleTizen *ble_tizen_;
  std::unique_ptr<FlEventSink> events_;
};

class BleStatusStreamHandler : public FlStreamHandler {
 public:
  explicit BleStatusStreamHandler(BleTizen *ble_tizen)
      : ble_tizen_(ble_tizen){};

 protected:
  std::unique_ptr<FlStreamHandlerError> OnListenInternal(
      const flutter::EncodableValue *arguments,
      std::unique_ptr<FlEventSink> &&events) override {
    events_ = std::move(events);
    auto ble_status = ble_tizen_->GetBleStatus();
    std::string status = "unknown";
    if (ble_status.has_value()) {
      if (ble_status == BleStatus::kEnabled) {
        status = "ready";
      } else if (ble_status == BleStatus::kDisabled) {
        status = "poweredOff";
      } else if (ble_status == BleStatus::kNotSupported) {
        status = "unsupported";
      }
    }
    events_->Success(flutter::EncodableValue(status));

    ble_tizen_->SetBleStatusChangedCallback([this](BleStatus ble_status) {
      std::string status =
          ble_status == BleStatus::kEnabled ? "ready" : "poweredOff";
      events_->Success(flutter::EncodableValue(status));
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
    auto plugin = std::make_unique<FlutterReactiveBleTizenPlugin>();

    auto method_channel = std::make_unique<FlMethodChannel>(
        registrar->messenger(), "flutter_reactive_ble_method",
        &flutter::StandardMethodCodec::GetInstance());
    method_channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto &call, auto result) {
          plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    plugin->SetUpEventChannels(registrar);

    registrar->AddPlugin(std::move(plugin));
  }

  FlutterReactiveBleTizenPlugin() : ble_tizen_(std::make_unique<BleTizen>()) {}

  virtual ~FlutterReactiveBleTizenPlugin() {}

 private:
  void SetUpEventChannels(flutter::PluginRegistrar *registrar) {
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

  void HandleMethodCall(const FlMethodCall &method_call,
                        std::unique_ptr<FlMethodResult> result) {
    const auto &method_name = method_call.method_name();

    if (method_name == "initialize") {
      if (!ble_tizen_->Initialize()) {
        result->Error(std::to_string(ble_tizen_->GetLastError()),
                      ble_tizen_->GetLastErrorString());
        return;
      };
      result->Success();
    } else if (method_name == "deinitialize") {
      if (!ble_tizen_->Deinitialize()) {
        result->Error(std::to_string(ble_tizen_->GetLastError()),
                      ble_tizen_->GetLastErrorString());
        return;
      };
      result->Success();
    } else if (method_name == "scanForDevices") {
      const auto *arguments =
          std::get_if<flutter::EncodableMap>(method_call.arguments());
      if (!arguments) {
        result->Error("Invalid arguments", "No arguments provided.");
        return;
      }
      flutter::EncodableList service_id_list;
      if (!GetValueFromEncodableMap(arguments, kServiceIds, service_id_list)) {
        result->Error("Invalid arguments", MissingArgumentError(kServiceIds));
        return;
      }
      std::vector<Uuid> service_ids;
      for (const flutter::EncodableValue &service_id : service_id_list) {
        if (std::holds_alternative<std::string>(service_id)) {
          service_ids.push_back(std::get<std::string>(service_id));
        }
      }

      if (!ble_tizen_->Scan(service_ids)) {
        result->Error(std::to_string(ble_tizen_->GetLastError()),
                      ble_tizen_->GetLastErrorString());
        return;
      };
      result->Success();
    } else if (method_name == "connectToDevice") {
      std::optional<std::string> device_id =
          GetDeviceIdArgument(method_call, result.get());
      if (!device_id.has_value()) {
        return;
      }

      if (!ble_tizen_->ConnectToDevice(device_id.value())) {
        result->Error(std::to_string(ble_tizen_->GetLastError()),
                      ble_tizen_->GetLastErrorString());
        return;
      };
      result->Success();
    } else if (method_name == "disconnectDevice") {
      std::optional<std::string> device_id =
          GetDeviceIdArgument(method_call, result.get());
      if (!device_id.has_value()) {
        return;
      }

      if (!ble_tizen_->DisconnectFromDevice(device_id.value())) {
        result->Error(std::to_string(ble_tizen_->GetLastError()),
                      ble_tizen_->GetLastErrorString());
        return;
      };
      result->Success();
    } else if (method_name == "discoverServices") {
      std::optional<std::string> device_id =
          GetDeviceIdArgument(method_call, result.get());
      if (!device_id.has_value()) {
        return;
      }

      std::optional<std::vector<DiscoveredService>> services =
          ble_tizen_->DiscoverServices(device_id.value());
      if (!services.has_value()) {
        result->Error(std::to_string(ble_tizen_->GetLastError()),
                      ble_tizen_->GetLastErrorString());
        return;
      }

      flutter::EncodableList service_list;
      for (const DiscoveredService &service : services.value()) {
        service_list.push_back(ServiceToEncodableValue(service));
      }
      result->Success(flutter::EncodableValue(service_list));
    } else if (method_name == "subscribeToNotifications") {
      std::optional<QualifiedCharacteristic> characteristic =
          GetQualifiedCharacteristicArgument(method_call, result.get());
      if (!characteristic.has_value()) {
        return;
      }

      if (!ble_tizen_->ListenNotifications(characteristic.value())) {
        result->Error(std::to_string(ble_tizen_->GetLastError()),
                      ble_tizen_->GetLastErrorString());
        return;
      }
      result->Success();
    } else if (method_name == "stopSubscribingToNotifications") {
      std::optional<QualifiedCharacteristic> characteristic =
          GetQualifiedCharacteristicArgument(method_call, result.get());
      if (!characteristic.has_value()) {
        return;
      }

      if (!ble_tizen_->StopNotifications(characteristic.value())) {
        result->Error(std::to_string(ble_tizen_->GetLastError()),
                      ble_tizen_->GetLastErrorString());
        return;
      }
      result->Success();
    } else if (method_name == "readCharacteristic") {
      std::optional<QualifiedCharacteristic> characteristic =
          GetQualifiedCharacteristicArgument(method_call, result.get());
      if (!characteristic.has_value()) {
        return;
      }

      if (!ble_tizen_->ReadCharacteristics(characteristic.value())) {
        result->Error(std::to_string(ble_tizen_->GetLastError()),
                      ble_tizen_->GetLastErrorString());
        return;
      }
      result->Success();
    } else if (method_name == "writeCharacteristicWithResponse") {
      // TODO
      result->NotImplemented();
    } else if (method_name == "writeCharacteristicWithoutResponse") {
      // TODO
      result->NotImplemented();
    } else {
      result->NotImplemented();
    }
  }

  std::optional<std::string> GetDeviceIdArgument(
      const FlMethodCall &method_call, FlMethodResult *result) {
    const auto *arguments =
        std::get_if<flutter::EncodableMap>(method_call.arguments());
    if (!arguments) {
      result->Error("Invalid arguments", "No arguments provided.");
      return std::nullopt;
    }
    std::string device_id;
    if (!GetValueFromEncodableMap(arguments, kDeviceId, device_id)) {
      result->Error("Invalid arguments", MissingArgumentError(kDeviceId));
      return std::nullopt;
    }
    return device_id;
  }

  std::optional<QualifiedCharacteristic> GetQualifiedCharacteristicArgument(
      const FlMethodCall &method_call, FlMethodResult *result) {
    const auto *arguments =
        std::get_if<flutter::EncodableMap>(method_call.arguments());
    if (!arguments) {
      result->Error("Invalid arguments", "No arguments provided.");
      return std::nullopt;
    }
    flutter::EncodableMap characteristic;
    if (!GetValueFromEncodableMap(arguments, kQualifiedCharacteristic,
                                  characteristic)) {
      result->Error("Invalid arguments",
                    MissingArgumentError(kQualifiedCharacteristic));
      return std::nullopt;
    }
    std::string device_id;
    if (!GetValueFromEncodableMap(&characteristic, kDeviceId, device_id)) {
      result->Error("Invalid arguments", MissingArgumentError(kDeviceId));
      return std::nullopt;
    }
    std::string service_id;
    if (!GetValueFromEncodableMap(&characteristic, kServiceId, service_id)) {
      result->Error("Invalid arguments", MissingArgumentError(kServiceId));
      return std::nullopt;
    }
    std::string characteristic_id;
    if (!GetValueFromEncodableMap(&characteristic, kCharacteristicId,
                                  characteristic_id)) {
      result->Error("Invalid arguments",
                    MissingArgumentError(kCharacteristicId));
      return std::nullopt;
    }
    return QualifiedCharacteristic(device_id, service_id, characteristic_id);
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

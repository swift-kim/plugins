import 'package:flutter/services.dart';
import 'package:reactive_ble_platform_interface/reactive_ble_platform_interface.dart';

const String kBleStatus = 'ble_status';
const String kCharacteristicId = 'characteristic_id';
const String kCharacteristicIds = 'characteristic_ids';
const String kCharacteristics = 'characteristics';
const String kConnectionState = 'connection_state';
const String kDeviceId = 'device_id';
const String kDeviceName = 'device_name';
const String kDiscoveredDevice = 'discovered_device';
const String kFailure = 'failure';
const String kIncludedServices = 'included_services';
const String kIsIndicatable = 'is_indicatable';
const String kIsNotifiable = 'is_notifiable';
const String kIsReadable = 'is_readable';
const String kIsWritableWithoutResponse = 'is_writable_without_response';
const String kIsWritableWithResponse = 'is_writable_with_response';
const String kManufacturerData = 'manufacturer_data';
const String kMessage = 'message';
const String kQualifiedCharacteristic = 'qualified_characteristic';
const String kResult = 'result';
const String kRssi = 'rssi';
const String kServiceData = 'service_data';
const String kServiceId = 'service_id';
const String kServiceIds = 'service_ids';
const String kServices = "services";
const String kValue = 'value';

extension CharacteristicValueExtension on CharacteristicValue {
  static CharacteristicValue fromMap(Map<String, Object> map) {
    final qualifiedCharacteristicMap =
        (map[kQualifiedCharacteristic] as Map).cast<String, Object>();
    final resultMap = map[kResult] as Map;
    final value = (resultMap[kValue] as List?)?.cast<int>();
    final failureMap = resultMap[kFailure] as Map?;

    if (value != null) {
      return CharacteristicValue(
        characteristic: QualifiedCharacteristicExtension.fromMap(
            qualifiedCharacteristicMap),
        result: Result.success(value),
      );
    } else if (failureMap != null) {
      return CharacteristicValue(
        characteristic: QualifiedCharacteristicExtension.fromMap(
            qualifiedCharacteristicMap),
        result: Result.failure(
          GenericFailure(
              code: CharacteristicValueUpdateError.unknown,
              message: failureMap[kMessage] as String),
        ),
      );
    }
    throw Exception();
  }
}

extension ConnectionStateUpdateExtension on ConnectionStateUpdate {
  static ConnectionStateUpdate fromMap(Map<String, Object> map) =>
      ConnectionStateUpdate(
        deviceId: map[kDeviceId] as String,
        connectionState: DeviceConnectionState.values
            .byName(map[kConnectionState] as String),
        failure: null,
      );
}

extension DiscoveredCharacteristicExtension on DiscoveredCharacteristic {
  static DiscoveredCharacteristic fromMap(Map<Object?, Object?> map) =>
      DiscoveredCharacteristic(
        characteristicId: Uuid.parse(map[kCharacteristicId] as String),
        serviceId: Uuid.parse(map[kServiceId] as String),
        isReadable: map[kIsReadable] as bool,
        isWritableWithResponse: map[kIsWritableWithResponse] as bool,
        isWritableWithoutResponse: map[kIsWritableWithoutResponse] as bool,
        isNotifiable: map[kIsNotifiable] as bool,
        isIndicatable: map[kIsIndicatable] as bool,
      );
}

extension DiscoveredDeviceExtension on DiscoveredDevice {
  static DiscoveredDevice fromMap(Map<String, Object> map) {
    final id = map[kDeviceId] as String;
    final name = map[kDeviceName] as String? ?? '';
    final serviceData = (map[kServiceData] as Map)
        .cast<String, Uint8List>()
        .map((String key, Uint8List value) => MapEntry(Uuid.parse(key), value));
    final manufacturerData = map[kManufacturerData] as Uint8List;
    final rssi = map[kRssi] as int;
    final serviceUuids =
        (map[kServiceIds] as List).cast<String>().map(Uuid.parse).toList();
    return DiscoveredDevice(
      id: id,
      name: name,
      serviceData: serviceData,
      manufacturerData: manufacturerData,
      rssi: rssi,
      serviceUuids: serviceUuids,
    );
  }
}

extension DiscoveredServiceExtension on DiscoveredService {
  static DiscoveredService fromMap(Map<Object?, Object?> map) =>
      DiscoveredService(
        serviceId: Uuid.parse(map[kServiceId] as String),
        characteristicIds: (map[kCharacteristicIds] as List)
            .cast<String>()
            .map(Uuid.parse)
            .toList(),
        includedServices: (map[kIncludedServices] as List)
            .cast<Map<Object?, Object?>>()
            .map(DiscoveredServiceExtension.fromMap)
            .toList(),
        characteristics: (map[kCharacteristics] as List)
            .cast<Map<Object?, Object?>>()
            .map(DiscoveredCharacteristicExtension.fromMap)
            .toList(),
      );
}

extension QualifiedCharacteristicExtension on QualifiedCharacteristic {
  Map<String, Object> toMap() => {
        kDeviceId: deviceId,
        kServiceId: '$serviceId',
        kCharacteristicId: '$characteristicId',
      };

  static QualifiedCharacteristic fromMap(Map<String, Object> map) =>
      QualifiedCharacteristic(
        deviceId: map[kDeviceId] as String,
        serviceId: Uuid.parse(map[kServiceId] as String),
        characteristicId: Uuid.parse(map[kCharacteristicId] as String),
      );
}

extension ScanResultExtension on ScanResult {
  static ScanResult fromMap(Map<String, Object> map) {
    final discoveredDeviceMap = map[kDiscoveredDevice] as Map?;
    final failureMap = map[kFailure] as Map?;

    if (discoveredDeviceMap != null) {
      return ScanResult(
          result: Result.success(DiscoveredDeviceExtension.fromMap(
              discoveredDeviceMap.cast<String, Object>())));
    } else if (failureMap != null) {
      return ScanResult(
        result: Result.failure(
          GenericFailure(
              code: ScanFailure.unknown,
              message: failureMap[kMessage] as String),
        ),
      );
    }
    throw Exception();
  }
}

extension WriteCharacteristicInfoExtension on WriteCharacteristicInfo {
  static WriteCharacteristicInfo fromMap(Map<String, Object> map) {
    final qualifiedCharacteristicMap =
        (map[kQualifiedCharacteristic] as Map).cast<String, Object>();
    final resultMap = map[kResult] as Map;
    final unit = resultMap[kValue] as bool?;
    final failureMap = resultMap[kFailure] as Map?;

    if (unit != null) {
      return WriteCharacteristicInfo(
        characteristic: QualifiedCharacteristicExtension.fromMap(
            qualifiedCharacteristicMap),
        result: const Result.success(Unit()),
      );
    } else if (failureMap != null) {
      return WriteCharacteristicInfo(
        characteristic: QualifiedCharacteristicExtension.fromMap(
            qualifiedCharacteristicMap),
        result: Result.failure(
          GenericFailure(
              code: WriteCharacteristicFailure.unknown,
              message: failureMap[kMessage] as String),
        ),
      );
    }
    throw Exception();
  }
}

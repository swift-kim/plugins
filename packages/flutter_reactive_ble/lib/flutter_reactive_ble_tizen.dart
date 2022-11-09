// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:reactive_ble_platform_interface/reactive_ble_platform_interface.dart';
import 'src/models.dart';

/// Tizen implementation of [ReactiveBlePlatform].
class ReactiveBleTizen extends ReactiveBlePlatform {
  /// Registers this class as the default instance of [ReactiveBlePlatform].
  static void register() {
    ReactiveBlePlatform.instance = ReactiveBleTizen();
  }

  final MethodChannel _methodChannel =
      const MethodChannel('flutter_reactive_ble_method');

  final _connectedDeviceChannel =
      const EventChannel("flutter_reactive_ble_connected_device");
  final _charEventChannel =
      const EventChannel("flutter_reactive_ble_char_update");
  final _scanEventChannel = const EventChannel("flutter_reactive_ble_scan");
  final _bleStatusChannel = const EventChannel("flutter_reactive_ble_status");

  @visibleForTesting
  MethodChannel get methodChannel => _methodChannel;

  Stream<ConnectionStateUpdate>? _connectionUpdateStream;
  Stream<CharacteristicValue>? _charValueStream;
  Stream<ScanResult>? _scanResultStream;
  Stream<BleStatus>? _bleStatusStream;

  @override
  Stream<ConnectionStateUpdate> get connectionUpdateStream =>
      _connectionUpdateStream ??= _connectedDeviceChannel
          .receiveBroadcastStream()
          .map((dynamic event) => ConnectionStateUpdateExtension.fromMap(
              (event as Map).cast<String, Object>()));

  @override
  Stream<CharacteristicValue> get charValueUpdateStream =>
      _charValueStream ??= _charEventChannel.receiveBroadcastStream().map(
          (dynamic event) => CharacteristicValueExtension.fromMap(
              (event as Map).cast<String, Object>()));

  @override
  Stream<ScanResult> get scanStream => _scanResultStream ??= _scanEventChannel
      .receiveBroadcastStream()
      .map((dynamic event) =>
          ScanResultExtension.fromMap((event as Map).cast<String, Object>()));

  @override
  Stream<BleStatus> get bleStatusStream =>
      _bleStatusStream ??= _bleStatusChannel.receiveBroadcastStream().map(
          (dynamic event) => BleStatus.values.byName(
              (event as Map).cast<String, Object>()[kBleStatus] as String));

  @override
  Future<void> initialize() => _methodChannel.invokeMethod('initialize');

  @override
  Future<void> deinitialize() => _methodChannel.invokeMethod('deinitialize');

  @override
  Stream<void> scanForDevices(
      {required List<Uuid> withServices,
      required ScanMode scanMode,
      // ignore: prefer_expression_function_bodies
      required bool requireLocationServicesEnabled}) {
    // Ignore `scanMode`.
    // Ignore `requireLocationServicesEnabled.`
    return _methodChannel.invokeMethod<void>(
      'scanForDevices',
      <String, Object>{
        kServiceIds: withServices.map((Uuid uuid) => '$uuid').toList(),
      },
    ).asStream();
  }

  @override
  Stream<void> connectToDevice(
      String id,
      Map<Uuid, List<Uuid>>? servicesWithCharacteristicsToDiscover,
      // ignore: prefer_expression_function_bodies
      Duration? connectionTimeout) {
    return _methodChannel.invokeMethod<void>(
      'connectToDevice',
      <String, Object>{kDeviceId: id},
    ).asStream();
  }

  @override
  Future<void> disconnectDevice(String deviceId) => _methodChannel
      .invokeMethod('disconnectDevice', <String, Object>{kDeviceId: deviceId});

  @override
  Future<List<DiscoveredService>> discoverServices(String deviceId) async {
    final dynamic discoverServices = await _methodChannel.invokeMethod<dynamic>(
        'discoverServices', <String, Object>{kDeviceId: deviceId});
    return (discoverServices[kServices] as List)
        .cast<Map<Object?, Object?>>()
        .map(DiscoveredServiceExtension.fromMap)
        .toList();
  }

  @override
  Future<ConnectionPriorityInfo> requestConnectionPriority(
      String deviceId, ConnectionPriority priority) {
    throw UnimplementedError();
  }

  @override
  Future<int> requestMtuSize(String deviceId, int? mtu) async {
    throw UnimplementedError();
  }

  @override
  Stream<void> subscribeToNotifications(
          QualifiedCharacteristic characteristic) =>
      _methodChannel.invokeMethod<void>(
          'subscribeToNotifications', <String, Object>{
        kQualifiedCharacteristic: characteristic.toMap()
      }).asStream();

  @override
  Future<void> stopSubscribingToNotifications(
          QualifiedCharacteristic characteristic) =>
      _methodChannel.invokeMethod('stopSubscribingToNotifications',
          <String, Object>{kQualifiedCharacteristic: characteristic.toMap()});

  @override
  Stream<void> readCharacteristic(QualifiedCharacteristic characteristic) =>
      _methodChannel.invokeMethod<void>('readCharacteristic', <String, Object>{
        kQualifiedCharacteristic: characteristic.toMap()
      }).asStream();

  @override
  Future<WriteCharacteristicInfo> writeCharacteristicWithResponse(
      QualifiedCharacteristic characteristic, List<int> value) async {
    final dynamic result = await _methodChannel.invokeMethod<dynamic>(
        'writeCharacteristicWithResponse', <String, Object>{
      kQualifiedCharacteristic: characteristic.toMap(),
      kValue: Uint8List.fromList(value),
    });
    return WriteCharacteristicInfoExtension.fromMap(
        (result as Map).cast<String, Object>());
  }

  @override
  Future<WriteCharacteristicInfo> writeCharacteristicWithoutResponse(
      QualifiedCharacteristic characteristic, List<int> value) async {
    final dynamic result = await _methodChannel.invokeMethod<dynamic>(
        'writeCharacteristicWithoutResponse', <String, Object>{
      kQualifiedCharacteristic: characteristic.toMap(),
      kValue: Uint8List.fromList(value),
    });
    return WriteCharacteristicInfoExtension.fromMap(
        (result as Map).cast<String, Object>());
  }
}

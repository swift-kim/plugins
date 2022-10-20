// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:reactive_ble_platform_interface/reactive_ble_platform_interface.dart';

/// Tizen implementation of [ReactiveBlePlatform].
class ReactiveBleTizen extends ReactiveBlePlatform {
  /// Registers this class as the default instance of [ReactiveBlePlatform].
  static void register() {
    ReactiveBlePlatform.instance = ReactiveBleTizen();
  }

  // The [StreamController]s used to send events to stream listeners.
  // These members are marked `late` to avoid the "bang operator" in implementaions.
  // They are always assigned with a valid object when [initialized] is `true`.
  late StreamController<ConnectionStateUpdate> connectionUpdateController;
  late StreamController<BleStatus> bleStatusController;
  late StreamController<CharacteristicValue> charValueUpdateController;
  late StreamController<ScanResult> scanController;

  @override
  Stream<BleStatus> get bleStatusStream {
    _ensureInitialized();
    return bleStatusController.stream;
  }

  @override
  Stream<ScanResult> get scanStream {
    _ensureInitialized();
    return scanController.stream;
  }

  @override
  Stream<CharacteristicValue> get charValueUpdateStream {
    _ensureInitialized();
    return charValueUpdateController.stream;
  }

  @override
  Stream<ConnectionStateUpdate> get connectionUpdateStream {
    _ensureInitialized();
    return connectionUpdateController.stream;
  }

  bool _initialized = false;

  bool get initialized => _initialized;

  void _ensureInitialized() {
    if (!_initialized) {
      throw Exception('ReactiveBlePlatform is not initialized.');
    }
  }

  @override
  Future<void> initialize() async {
    if (_initialized) {
      return;
    }
    connectionUpdateController = StreamController.broadcast();
    bleStatusController = StreamController.broadcast();
    charValueUpdateController = StreamController.broadcast();
    scanController = StreamController.broadcast();
    _initialized = true;
  }

  @override
  Future<void> deinitialize() async {
    if (!_initialized) {
      return;
    }
    unawaited(connectionUpdateController.close());
    unawaited(bleStatusController.close());
    unawaited(charValueUpdateController.close());
    unawaited(scanController.close());
    _initialized = false;
  }

  @override
  Stream<void> scanForDevices(
      {required List<Uuid> withServices,
      required ScanMode scanMode,
      required bool requireLocationServicesEnabled}) {
    // TODO: implement scanForDevices
    throw UnimplementedError();
  }

  @override
  Stream<void> connectToDevice(
      String id,
      Map<Uuid, List<Uuid>>? servicesWithCharacteristicsToDiscover,
      Duration? connectionTimeout) {
    // TODO: implement connectToDevice
    throw UnimplementedError();
  }

  @override
  Future<void> disconnectDevice(String deviceId) {
    // TODO: implement disconnectDevice
    throw UnimplementedError();
  }

  @override
  Future<List<DiscoveredService>> discoverServices(String deviceId) {
    // TODO: implement discoverServices
    throw UnimplementedError();
  }

  @override
  Future<ConnectionPriorityInfo> requestConnectionPriority(
      String deviceId, ConnectionPriority priority) {
    // TODO: implement requestConnectionPriority
    throw UnimplementedError();
  }

  @override
  Future<int> requestMtuSize(String deviceId, int? mtu) {
    // TODO: implement requestMtuSize
    throw UnimplementedError();
  }

  @override
  Stream<void> subscribeToNotifications(
      QualifiedCharacteristic characteristic) {
    // TODO: implement subscribeToNotifications
    throw UnimplementedError();
  }

  @override
  Future<void> stopSubscribingToNotifications(
      QualifiedCharacteristic characteristic) {
    // TODO: implement stopSubscribingToNotifications
    throw UnimplementedError();
  }

  @override
  Stream<void> readCharacteristic(QualifiedCharacteristic characteristic) {
    // TODO: implement readCharacteristic
    throw UnimplementedError();
  }

  @override
  Future<WriteCharacteristicInfo> writeCharacteristicWithResponse(
      QualifiedCharacteristic characteristic, List<int> value) {
    // TODO: implement writeCharacteristicWithResponse
    throw UnimplementedError();
  }

  @override
  Future<WriteCharacteristicInfo> writeCharacteristicWithoutResponse(
      QualifiedCharacteristic characteristic, List<int> value) {
    // TODO: implement writeCharacteristicWithoutResponse
    throw UnimplementedError();
  }
}

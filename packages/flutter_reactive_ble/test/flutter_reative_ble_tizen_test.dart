import 'package:flutter/services.dart';
import 'package:flutter_reactive_ble_tizen/flutter_reactive_ble_tizen.dart';
import 'package:flutter_reactive_ble_tizen/src/models.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:reactive_ble_platform_interface/reactive_ble_platform_interface.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  late ReactiveBleTizen plugin;
  late List<MethodCall> log;

  setUp(() {
    plugin = ReactiveBleTizen();
    log = <MethodCall>[];
    plugin.methodChannel.setMockMethodCallHandler((MethodCall methodCall) {
      log.add(methodCall);
      return null;
    });
  });

  test('registers instance', () {
    ReactiveBleTizen.register();
    expect(ReactiveBlePlatform.instance, isA<ReactiveBleTizen>());
  });

  test('scanForDevices', () {
    final uuids = [
      // 16bit uuid.
      Uuid.parse('0A0A'),
      // 32bit uuid.
      Uuid.parse('0A0A0A0A'),
      // 128bit uuid.
      Uuid.parse('0A0A0A0A-0A0A-0A0A-0A0A-0A0A0A0A0A0A'),
    ];

    plugin.scanForDevices(
        withServices: uuids,
        scanMode: ScanMode.balanced,
        requireLocationServicesEnabled: true);

    expect(log, <Matcher>[
      isMethodCall(
        'scanForDevices',
        arguments: <String, Object>{
          'withServices': <List<int>>[
            List<int>.filled(2, 10),
            List<int>.filled(4, 10),
            List<int>.filled(16, 10),
          ],
        },
      ),
    ]);
  });

  test('subscribeToNotifications', () {
    plugin.subscribeToNotifications(QualifiedCharacteristic(
      deviceId: '01:23:45:67:89:AB',
      // Battery Service UUID.
      serviceId: Uuid([0x18, 0x0f]),
      // Battery Level Characteristic UUID.
      characteristicId: Uuid([0x2a, 0x19]),
    ));

    expect(log, <Matcher>[
      isMethodCall(
        'subscribeToNotifications',
        arguments: <String, Object>{
          kQualifiedCharacteristic: <String, Object>{
            'device_id': '01:23:45:67:89:AB',
            'service_id': [0x18, 0x0f],
            'characteristic_id': [0x2a, 0x19],
          }
        },
      ),
    ]);
  });
}

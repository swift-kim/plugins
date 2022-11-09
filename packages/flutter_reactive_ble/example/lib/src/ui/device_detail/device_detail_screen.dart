import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:provider/provider.dart';

import '../../ble/ble_device_connector.dart';
import '../../ui/device_detail/device_log_tab.dart';

import 'device_interaction_tab.dart';

class DeviceDetailScreen extends StatelessWidget {
  const DeviceDetailScreen({required this.device, super.key});

  final DiscoveredDevice device;

  @override
  Widget build(BuildContext context) => Consumer<BleDeviceConnector>(
        builder: (_, deviceConnector, __) => _DeviceDetail(
          device: device,
          disconnect: deviceConnector.disconnect,
        ),
      );
}

/// A page that has two tab views:
///   - DeviceInteractionTab: shows device's ble information.
///   - DeviceLogTab: shows device's ble operations.
class _DeviceDetail extends StatelessWidget {
  const _DeviceDetail({
    required this.device,
    required this.disconnect,
  });

  final DiscoveredDevice device;
  final void Function(String deviceId) disconnect;
  
  @override
  Widget build(BuildContext context) => WillPopScope(
        onWillPop: () async {
          disconnect(device.id);
          return true;
        },
        child: DefaultTabController(
          length: 2,
          child: Scaffold(
            appBar: AppBar(
              title: Text(device.name),
              bottom: const TabBar(
                tabs: [
                  Tab(
                    icon: Icon(
                      Icons.bluetooth_connected,
                    ),
                  ),
                  Tab(
                    icon: Icon(
                      Icons.find_in_page_sharp,
                    ),
                  ),
                ],
              ),
            ),
            body: TabBarView(
              children: [
                DeviceInteractionTab(
                  device: device,
                ),
                const DeviceLogTab(),
              ],
            ),
          ),
        ),
      );
}

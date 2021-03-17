# wifi_info_flutter_tizen

The Tizen implementation of [`wifi_info_flutter`](https://github.com/flutter/plugins/tree/master/packages/wifi_info_flutter).

## Usage

This package is not an _endorsed_ implementation of `wifi_info_flutter`. Therefore, you have to include `wifi_info_flutter_tizen` alongside `wifi_info_flutter` as dependencies in your `pubspec.yaml` file.

```yaml
dependencies:
  wifi_info_flutter: ^1.0.1
  wifi_info_flutter_tizen: ^1.0.0
```

Then you can import `wifi_info_flutter` in your Dart code:

```dart
import 'package:wifi_info_flutter/wifi_info_flutter.dart';
```

For detailed usage, see https://github.com/flutter/plugins/tree/master/packages/wifi_info_flutter/wifi_info_flutter#usage.

## Supported devices

This plugin is supported on these types of devices:

- Galaxy Watch (running Tizen 4.0 or later)

## Required privileges

To get network information using this plugin, add below lines under the `<manifest>` section in your `tizen-manifest.xml` file,

```xml
<privileges>
  <privilege>http://tizen.org/privilege/network.get</privilege>
</privileges>
```

For details, see [Security and API Privileges](https://docs.tizen.org/application/dotnet/tutorials/sec-privileges).
# share_plus_tizen

[![pub package](https://img.shields.io/pub/v/share_plus_tizen.svg)](https://pub.dev/packages/share_plus_tizen)

---

## Deprecation Notice

This plugin is no longer maintained by the [flutter-tizen](https://github.com/flutter-tizen) team and no further updates will be made. You might consider migrating to the [`tizen_app_control`](https://pub.dev/packages/tizen_app_control) plugin.

---

The Tizen implementation of [`share_plus`](https://pub.dev/packages/share_plus).

## Usage

To use this plugin, add `share_plus` and `share_plus_tizen` as [dependencies in your pubspec.yaml file](https://flutter.io/platform-plugins/).

```yaml
dependencies:
  share_plus: ^3.0.5
  share_plus_tizen: ^1.1.2
```

Then you can import `share_plus` in your Dart code.

``` dart
import 'package:share_plus/share_plus.dart';

Share.share('check out my website https://example.com');
```

For detailed usage, see https://pub.dev/packages/share_plus#example.

## Supported devices

- Galaxy Watch series (running Tizen 4.0 or later)

You can send only **SMS messages** using this plugin. You can't use this plugin on TV devices because no SMS or e-mail app is available on them.

## Supported APIs

- [x] `Share.share` (no optional argument supported)
- [ ] `Share.shareFiles` (no e-mail app available)

## Required privileges

To use this plugin, you need to declare privileges in `tizen-manifest.xml` of your application.

``` xml
<privileges>
  <privilege>http://tizen.org/privilege/appmanager.launch</privilege>
</privileges>
```

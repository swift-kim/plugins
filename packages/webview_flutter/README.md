# webview_flutter_tizen

The Tizen implementation of [`webview_flutter`](https://github.com/flutter/plugins/tree/master/packages/webview_flutter).

## Supported devices

This plugin is available on these types of devices:

- Galaxy Watch or TV (running Tizen 5.5 or later)

## Required privileges

To use this plugin, add below lines under the `<manifest>` section in your `tizen-manifest.xml` file,

```xml
<privileges>
  <privilege>http://tizen.org/privilege/internet</privilege>
</privileges>
```

## Usage

This package is not an _endorsed_ implementation of `webview_flutter`. Therefore, you have to include `webview_flutter_tizen` alongside `webview_flutter` as dependencies in your `pubspec.yaml` file.

```yaml
dependencies:
  webview_flutter: ^1.0.6
  webview_flutter_tizen: ^0.1.0
```

## Example

```dart
import 'dart:io';
import 'package:webview_flutter/webview_flutter.dart';

class WebViewExample extends StatefulWidget {
  @override
  WebViewExampleState createState() => WebViewExampleState();
}

class WebViewExampleState extends State<WebViewExample> {
  @override
  void initState() {
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    return WebView(
      initialUrl: 'https://flutter.dev',
    );
  }
}
```

## Limitations
- This plugin is only supported on **Galaxy Watch and TV** devices running Tizen 5.5 or later.
- This is an initial webview plugin for Tizen and is implemented based on Tizen Lightweight Web Engine (LWE). If you would like to know detailed specifications that the LWE supports, please refer to the following link :
https://review.tizen.org/gerrit/gitweb?p=platform/upstream/lightweight-web-engine.git;a=blob;f=docs/Spec.md;h=ecb8f437c5a1facc77d3435e1a8aad6a267f12f3;hb=refs/heads/tizen

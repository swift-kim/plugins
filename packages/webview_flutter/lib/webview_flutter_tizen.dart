// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_tizen/services.dart';
import 'package:flutter_tizen/widgets.dart';

import 'package:webview_flutter_platform_interface/webview_flutter_platform_interface.dart';

import 'src/tizen_webview_controller.dart';

/// Implementation of [WebViewPlatform] using the EWK API.
class TizenWebViewPlatform extends WebViewPlatform {
  /// Registers this class as the default instance of [WebViewPlatform].
  static void registerWith() {
    WebViewPlatform.instance = TizenWebViewPlatform();
  }

  @override
  TizenWebViewController createPlatformWebViewController(
    PlatformWebViewControllerCreationParams params,
  ) {
    return TizenWebViewController(params);
  }

  @override
  TizenNavigationDelegate createPlatformNavigationDelegate(
    PlatformNavigationDelegateCreationParams params,
  ) {
    return TizenNavigationDelegate(params);
  }

  @override
  TizenWebViewWidget createPlatformWebViewWidget(
    PlatformWebViewWidgetCreationParams params,
  ) {
    return TizenWebViewWidget(params);
  }

  @override
  TizenWebViewCookieManager createPlatformCookieManager(
    PlatformWebViewCookieManagerCreationParams params,
  ) {
    return TizenWebViewCookieManager(params);
  }
}

/// An implementation of [PlatformWebViewWidget] with the EFL WebKit API.
class TizenWebViewWidget extends PlatformWebViewWidget {
  /// Constructs a [TizenWebViewWidget].
  TizenWebViewWidget(super.params) : super.implementation();

  @override
  Widget build(BuildContext context) {
    return PlatformViewLink(
      key: params.key,
      viewType: 'plugins.flutter.io/webview',
      surfaceFactory: (
        BuildContext context,
        PlatformViewController controller,
      ) {
        return TizenViewSurface(
          controller: controller as TizenViewController,
          gestureRecognizers: params.gestureRecognizers,
          hitTestBehavior: PlatformViewHitTestBehavior.opaque,
        );
      },
      onCreatePlatformView: (PlatformViewCreationParams params) {
        return _initTizenView(params)
          ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
          ..create();
      },
    );
  }

  TizenViewController _initTizenView(PlatformViewCreationParams view_params) {
    return PlatformViewsServiceTizen.initTizenView(
      id: view_params.id,
      viewType: 'plugins.flutter.io/webview',
      layoutDirection: params.layoutDirection,
      creationParams: widget.creationParams,
      creationParamsCodec: const StandardMessageCodec(),
      onFocus: () {
        // view_params.onFocusChanged(true);
        _focusNode!.requestFocus();
      },
    );
    if (displayWithHybridComposition) {
      return params.platformViewsServiceProxy.initExpensiveTizenView(
        id: view_params.id,
        viewType: 'plugins.flutter.io/webview',
        layoutDirection: params.layoutDirection,
        creationParams: params.instanceManager.getIdentifier(
            (_tizenParams.controller as TizenWebViewController)._webView),
        creationParamsCodec: const StandardMessageCodec(),
      );
    } else {
      return params.platformViewsServiceProxy.initSurfaceTizenView(
        id: view_params.id,
        viewType: 'plugins.flutter.io/webview',
        layoutDirection: params.layoutDirection,
        creationParams: params.instanceManager.getIdentifier(
            (params.controller as TizenWebViewController)._webView),
        creationParamsCodec: const StandardMessageCodec(),
      );
    }
  }
}

/// Builds a Tizen webview.
///
/// This is used as the default implementation for [WebView.platform] on Tizen. It uses a method channel to
/// communicate with the platform code.
class TizenWebView implements WebViewPlatform {
  /// Sets a tizen [WebViewPlatform].
  static void register() {
    WebView.platform = TizenWebView();
    WebViewCookieManagerPlatform.instance = WebViewTizenCookieManager();
  }

  @override
  Widget build({
    required BuildContext context,
    required CreationParams creationParams,
    required WebViewPlatformCallbacksHandler webViewPlatformCallbacksHandler,
    required JavascriptChannelRegistry javascriptChannelRegistry,
    WebViewPlatformCreatedCallback? onWebViewPlatformCreated,
    Set<Factory<OneSequenceGestureRecognizer>>? gestureRecognizers,
  }) {
    assert(webViewPlatformCallbacksHandler != null);
    return GestureDetector(
      onLongPress: () {},
      excludeFromSemantics: true,
      child: TizenView(
        viewType: 'plugins.flutter.io/webview',
        onPlatformViewCreated: (int id) {
          if (onWebViewPlatformCreated == null) {
            return;
          }
          onWebViewPlatformCreated(TizenWebViewController(
            id,
            webViewPlatformCallbacksHandler,
            javascriptChannelRegistry,
          ));
        },
        gestureRecognizers: gestureRecognizers,
        layoutDirection: Directionality.maybeOf(context) ?? TextDirection.rtl,
        creationParams:
            TizenWebViewController.creationParamsToMap(creationParams),
        creationParamsCodec: const StandardMessageCodec(),
      ),
    );
  }

  @override
  Future<bool> clearCookies() {
    if (WebViewCookieManagerPlatform.instance == null) {
      throw Exception(
          'Could not clear cookies as no implementation for WebViewCookieManagerPlatform has been registered.');
    }
    return WebViewCookieManagerPlatform.instance!.clearCookies();
  }
}

/// Handles all cookie operations for the current platform.
class TizenWebViewCookieManager extends PlatformWebViewCookieManager {
  /// Creates a new [TizenWebViewCookieManager].
  TizenWebViewCookieManager(super.params) : super.implementation();

  static const MethodChannel _cookieManagerChannel =
      MethodChannel('plugins.flutter.io/cookie_manager');

  @override
  Future<bool> clearCookies() {
    return _cookieManagerChannel
        .invokeMethod<bool>('clearCookies')
        .then<bool>((dynamic result) => result! as bool);
  }

  @override
  Future<void> setCookie(WebViewCookie cookie) async {
    if (!_isValidPath(cookie.path)) {
      throw ArgumentError(
          'The path property for the provided cookie was not given a legal value.');
    }
    return _cookieManagerChannel.invokeMethod<void>(
      'setCookie',
      <String, String>{
        'name': cookie.name,
        'value': cookie.value,
        'domain': cookie.domain,
        'path': cookie.path,
      },
    );
  }

  bool _isValidPath(String path) {
    // Permitted ranges based on RFC6265bis: https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-rfc6265bis-02#section-4.1.1
    for (final int char in path.codeUnits) {
      if ((char < 0x20 || char > 0x3A) && (char < 0x3C || char > 0x7E)) {
        return false;
      }
    }
    return true;
  }
}

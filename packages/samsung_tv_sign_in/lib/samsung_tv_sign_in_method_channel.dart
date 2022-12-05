import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'samsung_tv_sign_in_platform_interface.dart';

/// An implementation of [SamsungTvSignInPlatform] that uses method channels.
class MethodChannelSamsungTvSignIn extends SamsungTvSignInPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('samsung_tv_sign_in');

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>('getPlatformVersion');
    return version;
  }
}

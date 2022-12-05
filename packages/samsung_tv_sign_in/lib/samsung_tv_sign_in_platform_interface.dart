import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'samsung_tv_sign_in_method_channel.dart';

abstract class SamsungTvSignInPlatform extends PlatformInterface {
  /// Constructs a SamsungTvSignInPlatform.
  SamsungTvSignInPlatform() : super(token: _token);

  static final Object _token = Object();

  static SamsungTvSignInPlatform _instance = MethodChannelSamsungTvSignIn();

  /// The default instance of [SamsungTvSignInPlatform] to use.
  ///
  /// Defaults to [MethodChannelSamsungTvSignIn].
  static SamsungTvSignInPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [SamsungTvSignInPlatform] when
  /// they register themselves.
  static set instance(SamsungTvSignInPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }
}

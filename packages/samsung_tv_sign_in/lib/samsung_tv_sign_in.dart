// import 'samsung_tv_sign_in_platform_interface.dart';

import 'dart:ffi';

import 'package:ffi/ffi.dart';

// struct LoginInfo
// {
//   [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
//   public string LoginId;
//   [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
//   public string LoginPwd;
//   [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
//   public string GUID;
//   [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
//   public string Uid;
//   [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 1024)]
//   public string UserIcon;
// }

class SamsungTvSignIn {
  Future<bool> getLoginState() async {
    final DynamicLibrary ssoApiLib =
        DynamicLibrary.open('/usr/lib/libsso_api.so');
    final ssoGetLoginState = ssoApiLib.lookupFunction<
        Int Function(Pointer<Int>),
        int Function(Pointer<Int>)>('sso_get_login_state');
    return using((Arena arena) {
      Pointer<Int> state = arena();
      int ret = ssoGetLoginState(state);
      if (ret != 0) {
        print('Error: $ret');
        return false;
      }
      return state.value == 1;
    });
  }

  Future<String> getLoginInfo() async {
    // await SamsungTvSignInPlatform.instance.getPlatformVersion();
    return 'Hello';
  }
}

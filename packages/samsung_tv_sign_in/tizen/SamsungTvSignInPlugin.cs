using System;
using Tizen.Flutter.Embedding;
using Tizen.System;

namespace SamsungTvSignIn
{
    public class SamsungTvSignInPlugin : IFlutterPlugin
    {
        private MethodChannel _channel;

        public void OnAttachedToEngine(IFlutterPluginBinding binding)
        {
            _channel = new MethodChannel("samsung_tv_sign_in", StandardMethodCodec.Instance, binding.BinaryMessenger);
            _channel.SetMethodCallHandler(HandleMethodCall);
        }

        public void OnDetachedFromEngine()
        {
            _channel.UnsetMethodCallHandler();
            _channel = null;
        }

        public object HandleMethodCall(MethodCall call)
        {
            if (call.Method == "getPlatformVersion")
            {
                if (Information.TryGetValue<string>("http://tizen.org/feature/platform.version", out string version))
                {
                    return version;
                }
                else
                {
                    return "Unknown";
                }
            }
            throw new MissingPluginException();
        }
    }
}

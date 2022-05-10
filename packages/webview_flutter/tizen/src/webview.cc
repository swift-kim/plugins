// Copyright 2021 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webview.h"

#include <app_common.h>
#include <flutter/standard_method_codec.h>
#include <flutter_texture_registrar.h>
#include <system_info.h>
#include <tbm_surface.h>

#include <stdexcept>
#include <variant>

#include "buffer_pool.h"
#include "log.h"
#include "lwe/LWEWebView.h"
#include "lwe/PlatformIntegrationData.h"
#include "webview_factory.h"

static const int32_t kBufferPoolSize = 5;

extern "C" size_t LWE_EXPORT createWebViewInstance(
    unsigned x, unsigned y, unsigned width, unsigned height,
    float devicePixelRatio, const char* defaultFontName, const char* locale,
    const char* timezoneID,
    const std::function<::LWE::WebContainer::ExternalImageInfo(void)>&
        prepareImageCb,
    const std::function<void(::LWE::WebContainer*, bool needsFlush)>& flushCb,
    bool useSWBackend);

class NavigationRequestResult
    : public flutter::MethodResult<flutter::EncodableValue> {
 public:
  NavigationRequestResult(std::string url, WebView* webview)
      : url_(url), webview_(webview) {}

  void SuccessInternal(const flutter::EncodableValue* should_load) override {
    if (std::holds_alternative<bool>(*should_load)) {
      if (std::get<bool>(*should_load)) {
        LoadUrl();
      }
    }
  }

  void ErrorInternal(const std::string& error_code,
                     const std::string& error_message,
                     const flutter::EncodableValue* error_details) override {
    LOG_ERROR("The request unexpectedly completed with an error.");
  }

  void NotImplementedInternal() override {}

 private:
  void LoadUrl() {
    if (webview_->GetWebViewInstance()) {
      webview_->GetWebViewInstance()->LoadURL(url_);
    }
  }

  std::string url_;
  WebView* webview_;
};

enum ResourceErrorType {
  NoError,
  UnknownError,
  HostLookupError,
  UnsupportedAuthSchemeError,
  AuthenticationError,
  ProxyAuthenticationError,
  ConnectError,
  IOError,
  TimeoutError,
  RedirectLoopError,
  UnsupportedSchemeError,
  FailedSSLHandshakeError,
  BadURLError,
  FileError,
  FileNotFoundError,
  TooManyRequestError,
};

static std::string ErrorCodeToString(int error_code) {
  switch (error_code) {
    case ResourceErrorType::AuthenticationError:
      return "authentication";
    case ResourceErrorType::BadURLError:
      return "badUrl";
    case ResourceErrorType::ConnectError:
      return "connect";
    case ResourceErrorType::FailedSSLHandshakeError:
      return "failedSslHandshake";
    case ResourceErrorType::FileError:
      return "file";
    case ResourceErrorType::FileNotFoundError:
      return "fileNotFound";
    case ResourceErrorType::HostLookupError:
      return "hostLookup";
    case ResourceErrorType::IOError:
      return "io";
    case ResourceErrorType::ProxyAuthenticationError:
      return "proxyAuthentication";
    case ResourceErrorType::RedirectLoopError:
      return "redirectLoop";
    case ResourceErrorType::TimeoutError:
      return "timeout";
    case ResourceErrorType::TooManyRequestError:
      return "tooManyRequests";
    case ResourceErrorType::UnknownError:
      return "unknown";
    case ResourceErrorType::UnsupportedAuthSchemeError:
      return "unsupportedAuthScheme";
    case ResourceErrorType::UnsupportedSchemeError:
      return "unsupportedScheme";
    default:
      LOG_ERROR("Unknown error type: %d", error_code);
      return std::to_string(error_code);
  }
}

template <typename T>
static bool GetValueFromEncodableMap(const flutter::EncodableValue& arguments,
                                     std::string key, T* out) {
  if (auto map = std::get_if<flutter::EncodableMap>(&arguments)) {
    auto iter = map->find(flutter::EncodableValue(key));
    if (iter != map->end() && !iter->second.IsNull()) {
      if (auto* value = std::get_if<T>(&iter->second)) {
        *out = *value;
        return true;
      }
    }
  }
  return false;
}

static bool IsRunningOnEmulator() {
  bool result = false;
  char* value = nullptr;
  int ret = system_info_get_platform_string(
      "http://tizen.org/system/model_name", &value);
  if (ret == SYSTEM_INFO_ERROR_NONE && strcmp(value, "Emulator") == 0) {
    result = true;
  }
  if (value) {
    free(value);
  }
  return result;
}

WebView::WebView(flutter::PluginRegistrar* registrar, int view_id,
                 flutter::TextureRegistrar* texture_registrar, double width,
                 double height, const flutter::EncodableValue& params)
    : PlatformView(registrar, view_id, nullptr),
      texture_registrar_(texture_registrar),
      width_(width),
      height_(height) {
  use_sw_backend_ = IsRunningOnEmulator();
  if (use_sw_backend_) {
    tbm_pool_ = std::make_unique<SingleBufferPool>(width, height);
  } else {
    tbm_pool_ = std::make_unique<BufferPool>(width, height, kBufferPoolSize);
  }

  texture_variant_ =
      std::make_unique<flutter::TextureVariant>(flutter::GpuBufferTexture(
          [this](size_t width,
                 size_t height) -> const FlutterDesktopGpuBuffer* {
            return ObtainGpuBuffer(width, height);
          }));
  SetTextureId(texture_registrar_->RegisterTexture(texture_variant_.get()));

  InitWebView();

  channel_ = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
      GetPluginRegistrar()->messenger(), GetChannelName(),
      &flutter::StandardMethodCodec::GetInstance());
  channel_->SetMethodCallHandler(
      [webview = this](const auto& call, auto result) {
        webview->HandleMethodCall(call, std::move(result));
      });

  auto cookie_channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          GetPluginRegistrar()->messenger(),
          "plugins.flutter.io/cookie_manager",
          &flutter::StandardMethodCodec::GetInstance());
  cookie_channel->SetMethodCallHandler(
      [webview = this](const auto& call, auto result) {
        webview->HandleCookieMethodCall(call, std::move(result));
      });

  std::string url;
  if (!GetValueFromEncodableMap(params, "initialUrl", &url)) {
    url = "about:blank";
  }

  int32_t color;
  if (GetValueFromEncodableMap(params, "backgroundColor", &color)) {
    LWE::Settings settings = webview_instance_->GetSettings();
    settings.SetBaseBackgroundColor(color >> 16 & 0xff, color >> 8 & 0xff,
                                    color & 0xff, color >> 24 & 0xff);
    webview_instance_->SetSettings(settings);
  }

  flutter::EncodableMap settings;
  if (GetValueFromEncodableMap(params, "settings", &settings)) {
    ApplySettings(settings);
  }

  flutter::EncodableList names;
  if (GetValueFromEncodableMap(params, "javascriptChannelNames", &names)) {
    for (flutter::EncodableValue name : names) {
      if (std::holds_alternative<std::string>(name)) {
        RegisterJavaScriptChannelName(std::get<std::string>(name));
      }
    }
  }

  // TODO: Implement autoMediaPlaybackPolicy.

  std::string user_agent;
  if (GetValueFromEncodableMap(params, "userAgent", &user_agent)) {
    LWE::Settings settings = webview_instance_->GetSettings();
    settings.SetUserAgentString(user_agent);
    webview_instance_->SetSettings(settings);
  }

  webview_instance_->RegisterOnPageStartedHandler(
      [this](LWE::WebContainer* container, const std::string& url) {
        flutter::EncodableMap args = {
            {flutter::EncodableValue("url"), flutter::EncodableValue(url)}};
        channel_->InvokeMethod("onPageStarted",
                               std::make_unique<flutter::EncodableValue>(args));
      });

  webview_instance_->RegisterOnPageLoadedHandler(
      [this](LWE::WebContainer* container, const std::string& url) {
        flutter::EncodableMap args = {
            {flutter::EncodableValue("url"), flutter::EncodableValue(url)}};
        channel_->InvokeMethod("onPageFinished",
                               std::make_unique<flutter::EncodableValue>(args));
      });

  webview_instance_->RegisterOnProgressChangedHandler(
      [this](LWE::WebContainer* container, int progress) {
        if (!has_progress_tracking_) {
          return;
        }
        flutter::EncodableMap args = {{flutter::EncodableValue("progress"),
                                       flutter::EncodableValue(progress)}};
        channel_->InvokeMethod("onProgress",
                               std::make_unique<flutter::EncodableValue>(args));
      });

  webview_instance_->RegisterOnReceivedErrorHandler(
      [this](LWE::WebContainer* container, LWE::ResourceError error) {
        flutter::EncodableMap args = {
            {flutter::EncodableValue("errorCode"),
             flutter::EncodableValue(error.GetErrorCode())},
            {flutter::EncodableValue("description"),
             flutter::EncodableValue(error.GetDescription())},
            {flutter::EncodableValue("errorType"),
             flutter::EncodableValue(ErrorCodeToString(error.GetErrorCode()))},
            {flutter::EncodableValue("failingUrl"),
             flutter::EncodableValue(error.GetUrl())},
        };
        channel_->InvokeMethod("onWebResourceError",
                               std::make_unique<flutter::EncodableValue>(args));
      });

  webview_instance_->RegisterShouldOverrideUrlLoadingHandler(
      [this](LWE::WebContainer* view, const std::string& url) -> bool {
        if (!has_navigation_delegate_) {
          return false;
        }
        flutter::EncodableMap args = {
            {flutter::EncodableValue("url"), flutter::EncodableValue(url)},
            {flutter::EncodableValue("isForMainFrame"),
             flutter::EncodableValue(true)},
        };
        auto result = std::make_unique<NavigationRequestResult>(url, this);
        channel_->InvokeMethod("navigationRequest",
                               std::make_unique<flutter::EncodableValue>(args),
                               std::move(result));
        return true;
      });

  webview_instance_->LoadURL(url);
}

void WebView::ApplySettings(const flutter::EncodableMap& settings) {
  for (const auto& item : settings) {
    if (std::holds_alternative<std::string>(item.first)) {
      std::string key = std::get<std::string>(item.first);
      flutter::EncodableValue value = item.second;

      if (key == "jsMode") {
        // NOTE: Not supported by LWE on Tizen.
      } else if (key == "hasNavigationDelegate") {
        if (std::holds_alternative<bool>(value)) {
          has_navigation_delegate_ = std::get<bool>(value);
        }
      } else if (key == "hasProgressTracking") {
        if (std::holds_alternative<bool>(value)) {
          has_progress_tracking_ = std::get<bool>(value);
        }
      } else if (key == "debuggingEnabled") {
        // NOTE: Not supported by LWE on Tizen.
      } else if (key == "gestureNavigationEnabled") {
        // NOTE: Not supported by LWE on Tizen.
      } else if (key == "allowsInlineMediaPlayback") {
        // no-op inline media playback is always allowed on Tizen.
      } else if (key == "userAgent") {
        if (std::holds_alternative<std::string>(value)) {
          LWE::Settings settings = webview_instance_->GetSettings();
          settings.SetUserAgentString(std::get<std::string>(value));
          webview_instance_->SetSettings(settings);
        }
      } else if (key == "zoomEnabled") {
        // NOTE: Not supported by LWE on Tizen.
      } else {
        LOG_ERROR("Unknown settings key: %s", key.c_str());
      }
    }
  }
}

/**
 * Added as a JavaScript interface to the WebView for any JavaScript channel
 * that the Dart code sets up.
 *
 * Exposes a single method named `postMessage` to JavaScript, which sends a
 * message over a method channel to the Dart code.
 */
void WebView::RegisterJavaScriptChannelName(const std::string& name) {
  LOG_DEBUG("Register a JavaScript channel: %s", name.c_str());

  auto on_message = [this, name](const std::string& message) -> std::string {
    LOG_DEBUG("JavaScript channel message: %s", message.c_str());
    flutter::EncodableMap args = {
        {flutter::EncodableValue("channel"), flutter::EncodableValue(name)},
        {flutter::EncodableValue("message"), flutter::EncodableValue(message)},
    };
    channel_->InvokeMethod("javascriptChannelMessage",
                           std::make_unique<flutter::EncodableValue>(args));
    return "success";
  };
  webview_instance_->AddJavaScriptInterface(name, "postMessage", on_message);
}

WebView::~WebView() { Dispose(); }

std::string WebView::GetChannelName() {
  return "plugins.flutter.io/webview_" + std::to_string(GetViewId());
}

void WebView::Dispose() {
  texture_registrar_->UnregisterTexture(GetTextureId());

  if (webview_instance_) {
    webview_instance_->Destroy();
    webview_instance_ = nullptr;
  }
}

void WebView::Resize(double width, double height) {
  width_ = width;
  height_ = height;

  if (candidate_surface_) {
    candidate_surface_ = nullptr;
  }

  tbm_pool_->Prepare(width_, height_);
  webview_instance_->ResizeTo(width_, height_);
}

void WebView::Touch(int type, int button, double x, double y, double dx,
                    double dy) {
  if (type == 0) {  // down event
    webview_instance_->DispatchMouseDownEvent(
        LWE::MouseButtonValue::LeftButton,
        LWE::MouseButtonsValue::LeftButtonDown, x, y);
    is_mouse_lbutton_down_ = true;
  } else if (type == 1) {  // move event
    webview_instance_->DispatchMouseMoveEvent(
        is_mouse_lbutton_down_ ? LWE::MouseButtonValue::LeftButton
                               : LWE::MouseButtonValue::NoButton,
        is_mouse_lbutton_down_ ? LWE::MouseButtonsValue::LeftButtonDown
                               : LWE::MouseButtonsValue::NoButtonDown,
        x, y);
  } else if (type == 2) {  // up event
    webview_instance_->DispatchMouseUpEvent(
        LWE::MouseButtonValue::NoButton, LWE::MouseButtonsValue::NoButtonDown,
        x, y);
    is_mouse_lbutton_down_ = false;
  } else {
    LOG_WARN("Unknown touch event type: %d", type);
  }
}

static LWE::KeyValue KeyNameToKeyValue(const std::string& ecore_key,
                                       bool is_shift_pressed) {
  if (ecore_key == "Left") {
    return LWE::KeyValue::ArrowLeftKey;
  } else if (ecore_key == "Right") {
    return LWE::KeyValue::ArrowRightKey;
  } else if (ecore_key == "Up") {
    return LWE::KeyValue::ArrowUpKey;
  } else if (ecore_key == "Down") {
    return LWE::KeyValue::ArrowDownKey;
  } else if (ecore_key == "space") {
    return LWE::KeyValue::SpaceKey;
  } else if (ecore_key == "Select") {
    return LWE::KeyValue::EnterKey;
  } else if (ecore_key == "Return") {
    return LWE::KeyValue::EnterKey;
  } else if (ecore_key == "Tab") {
    return LWE::KeyValue::TabKey;
  } else if (ecore_key == "BackSpace") {
    return LWE::KeyValue::BackspaceKey;
  } else if (ecore_key == "Escape") {
    return LWE::KeyValue::EscapeKey;
  } else if (ecore_key == "Delete") {
    return LWE::KeyValue::DeleteKey;
  } else if (ecore_key == "at") {
    return LWE::KeyValue::AtMarkKey;
  } else if (ecore_key == "minus") {
    if (is_shift_pressed) {
      return LWE::KeyValue::UnderScoreMarkKey;
    } else {
      return LWE::KeyValue::MinusMarkKey;
    }
  } else if (ecore_key == "equal") {
    if (is_shift_pressed) {
      return LWE::KeyValue::PlusMarkKey;
    } else {
      return LWE::KeyValue::EqualitySignKey;
    }
  } else if (ecore_key == "bracketleft") {
    if (is_shift_pressed) {
      return LWE::KeyValue::LeftCurlyBracketMarkKey;
    } else {
      return LWE::KeyValue::LeftSquareBracketKey;
    }
  } else if (ecore_key == "bracketright") {
    if (is_shift_pressed) {
      return LWE::KeyValue::RightCurlyBracketMarkKey;
    } else {
      return LWE::KeyValue::RightSquareBracketKey;
    }
  } else if (ecore_key == "semicolon") {
    if (is_shift_pressed) {
      return LWE::KeyValue::ColonMarkKey;
    } else {
      return LWE::KeyValue::SemiColonMarkKey;
    }
  } else if (ecore_key == "apostrophe") {
    if (is_shift_pressed) {
      return LWE::KeyValue::DoubleQuoteMarkKey;
    } else {
      return LWE::KeyValue::SingleQuoteMarkKey;
    }
  } else if (ecore_key == "comma") {
    if (is_shift_pressed) {
      return LWE::KeyValue::LessThanMarkKey;
    } else {
      return LWE::KeyValue::CommaMarkKey;
    }
  } else if (ecore_key == "period") {
    if (is_shift_pressed) {
      return LWE::KeyValue::GreaterThanSignKey;
    } else {
      return LWE::KeyValue::PeriodKey;
    }
  } else if (ecore_key == "slash") {
    if (is_shift_pressed) {
      return LWE::KeyValue::QuestionMarkKey;
    } else {
      return LWE::KeyValue::SlashKey;
    }
  } else if (ecore_key.length() == 1) {
    const char ch = ecore_key.at(0);
    if (ch >= '0' && ch <= '9') {
      if (is_shift_pressed) {
        switch (ch) {
          case '1':
            return LWE::KeyValue::ExclamationMarkKey;
          case '2':
            return LWE::KeyValue::AtMarkKey;
          case '3':
            return LWE::KeyValue::SharpMarkKey;
          case '4':
            return LWE::KeyValue::DollarMarkKey;
          case '5':
            return LWE::KeyValue::PercentMarkKey;
          case '6':
            return LWE::KeyValue::CaretMarkKey;
          case '7':
            return LWE::KeyValue::AmpersandMarkKey;
          case '8':
            return LWE::KeyValue::AsteriskMarkKey;
          case '9':
            return LWE::KeyValue::LeftParenthesisMarkKey;
          case '0':
            return LWE::KeyValue::RightParenthesisMarkKey;
        }
      }
      return LWE::KeyValue(LWE::KeyValue::Digit0Key + ch - '0');
    } else if (ch >= 'a' && ch <= 'z') {
      if (is_shift_pressed) {
        return LWE::KeyValue(LWE::KeyValue::LowerAKey + ch - 'a' - 32);
      } else {
        return LWE::KeyValue(LWE::KeyValue::LowerAKey + ch - 'a');
      }
    } else if (ch >= 'A' && ch <= 'Z') {
      if (is_shift_pressed) {
        return LWE::KeyValue(LWE::KeyValue::AKey + ch - 'A' + 32);
      } else {
        return LWE::KeyValue(LWE::KeyValue::AKey + ch - 'A');
      }
    }
  } else if (ecore_key == "XF86AudioRaiseVolume") {
    return LWE::KeyValue::TVVolumeUpKey;
  } else if (ecore_key == "XF86AudioLowerVolume") {
    return LWE::KeyValue::TVVolumeDownKey;
  } else if (ecore_key == "XF86AudioMute") {
    return LWE::KeyValue::TVMuteKey;
  } else if (ecore_key == "XF86RaiseChannel") {
    return LWE::KeyValue::TVChannelUpKey;
  } else if (ecore_key == "XF86LowerChannel") {
    return LWE::KeyValue::TVChannelDownKey;
  } else if (ecore_key == "XF86AudioRewind") {
    return LWE::KeyValue::MediaTrackPreviousKey;
  } else if (ecore_key == "XF86AudioNext") {
    return LWE::KeyValue::MediaTrackNextKey;
  } else if (ecore_key == "XF86AudioPause") {
    return LWE::KeyValue::MediaPauseKey;
  } else if (ecore_key == "XF86AudioRecord") {
    return LWE::KeyValue::MediaRecordKey;
  } else if (ecore_key == "XF86AudioPlay") {
    return LWE::KeyValue::MediaPlayKey;
  } else if (ecore_key == "XF86AudioStop") {
    return LWE::KeyValue::MediaStopKey;
  } else if (ecore_key == "XF86Info") {
    return LWE::KeyValue::TVInfoKey;
  } else if (ecore_key == "XF86Back") {
    return LWE::KeyValue::TVReturnKey;
  } else if (ecore_key == "XF86Red") {
    return LWE::KeyValue::TVRedKey;
  } else if (ecore_key == "XF86Green") {
    return LWE::KeyValue::TVGreenKey;
  } else if (ecore_key == "XF86Yellow") {
    return LWE::KeyValue::TVYellowKey;
  } else if (ecore_key == "XF86Blue") {
    return LWE::KeyValue::TVBlueKey;
  } else if (ecore_key == "XF86SysMenu") {
    return LWE::KeyValue::TVMenuKey;
  } else if (ecore_key == "XF86Home") {
    return LWE::KeyValue::TVHomeKey;
  } else if (ecore_key == "XF86Exit") {
    return LWE::KeyValue::TVExitKey;
  } else if (ecore_key == "XF86PreviousChannel") {
    return LWE::KeyValue::TVPreviousChannel;
  } else if (ecore_key == "XF86ChannelList") {
    return LWE::KeyValue::TVChannelList;
  } else if (ecore_key == "XF86ChannelGuide") {
    return LWE::KeyValue::TVChannelGuide;
  } else if (ecore_key == "XF86SimpleMenu") {
    return LWE::KeyValue::TVSimpleMenu;
  } else if (ecore_key == "XF86EManual") {
    return LWE::KeyValue::TVEManual;
  } else if (ecore_key == "XF86ExtraApp") {
    return LWE::KeyValue::TVExtraApp;
  } else if (ecore_key == "XF86Search") {
    return LWE::KeyValue::TVSearch;
  } else if (ecore_key == "XF86PictureSize") {
    return LWE::KeyValue::TVPictureSize;
  } else if (ecore_key == "XF86Sleep") {
    return LWE::KeyValue::TVSleep;
  } else if (ecore_key == "XF86Caption") {
    return LWE::KeyValue::TVCaption;
  } else if (ecore_key == "XF86More") {
    return LWE::KeyValue::TVMore;
  } else if (ecore_key == "XF86BTVoice") {
    return LWE::KeyValue::TVBTVoice;
  } else if (ecore_key == "XF86Color") {
    return LWE::KeyValue::TVColor;
  } else if (ecore_key == "XF86PlayBack") {
    return LWE::KeyValue::TVPlayBack;
  }
  LOG_WARN("Unknown key name: %s", ecore_key.c_str());
  return LWE::KeyValue::UnidentifiedKey;
}

void WebView::DispatchKeyDownEvent(Ecore_Event_Key* key_event) {
  if (!IsFocused()) {
    return;
  }

  std::string key_name = key_event->keyname;
  bool is_shift_pressed = key_event->modifiers & 1;

  struct Param {
    LWE::WebContainer* webview_instance;
    LWE::KeyValue key_value;
  };
  Param* param = new Param();
  param->webview_instance = webview_instance_;
  param->key_value = KeyNameToKeyValue(key_name, is_shift_pressed);

  webview_instance_->AddIdleCallback(
      [](void* data) {
        Param* param = reinterpret_cast<Param*>(data);
        param->webview_instance->DispatchKeyDownEvent(param->key_value);
        param->webview_instance->DispatchKeyPressEvent(param->key_value);
        delete param;
      },
      param);
}

void WebView::DispatchKeyUpEvent(Ecore_Event_Key* key_event) {
  if (!IsFocused()) {
    return;
  }

  std::string key_name = key_event->keyname;
  bool is_shift_pressed = key_event->modifiers & 1;

  struct Param {
    LWE::WebContainer* webview_instance;
    LWE::KeyValue key_value;
  };
  Param* param = new Param();
  param->webview_instance = webview_instance_;
  param->key_value = KeyNameToKeyValue(key_name, is_shift_pressed);

  webview_instance_->AddIdleCallback(
      [](void* data) {
        Param* param = reinterpret_cast<Param*>(data);
        param->webview_instance->DispatchKeyUpEvent(param->key_value);
        delete param;
      },
      param);
}

void WebView::SetDirection(int direction) {
  // TODO: Implement if necessary.
}

void WebView::InitWebView() {
  if (webview_instance_) {
    webview_instance_->Destroy();
    webview_instance_ = nullptr;
  }

  float scale_factor = 1;

  webview_instance_ = (LWE::WebContainer*)createWebViewInstance(
      0, 0, width_, height_, scale_factor, "SamsungOneUI", "ko-KR",
      "Asia/Seoul",
      [this]() -> LWE::WebContainer::ExternalImageInfo {
        std::lock_guard<std::mutex> lock(mutex_);
        LWE::WebContainer::ExternalImageInfo result;
        if (!working_surface_) {
          working_surface_ = tbm_pool_->GetAvailableBuffer();
        }
        if (working_surface_) {
          result.imageAddress = static_cast<void*>(working_surface_->Surface());
        } else {
          result.imageAddress = nullptr;
        }
        return result;
      },
      [this](LWE::WebContainer* c, bool isRendered) {
        if (isRendered) {
          std::lock_guard<std::mutex> lock(mutex_);
          if (candidate_surface_) {
            tbm_pool_->Release(candidate_surface_);
            candidate_surface_ = nullptr;
          }
          candidate_surface_ = working_surface_;
          working_surface_ = nullptr;
          texture_registrar_->MarkTextureFrameAvailable(GetTextureId());
        }
      },
      use_sw_backend_);

#ifndef TV_PROFILE
  LWE::Settings settings = webview_instance_->GetSettings();
  settings.SetUserAgentString(
      "Mozilla/5.0 (like Gecko/54.0 Firefox/54.0) Mobile");
  webview_instance_->SetSettings(settings);
#endif
}

void WebView::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!webview_instance_ || !method_call.arguments()) {
    return;
  }
  const auto method_name = method_call.method_name();
  const auto& arguments = *method_call.arguments();

  LOG_DEBUG("HandleMethodCall: %s", method_name.c_str());

  if (method_name.compare("loadUrl") == 0) {
    std::string url;
    if (GetValueFromEncodableMap(arguments, "url", &url)) {
      webview_instance_->LoadURL(url);
      result->Success();
      return;
    }
    result->Error("InvalidArguments", "Please set 'url' properly");
  } else if (method_name.compare("updateSettings") == 0) {
    if (std::holds_alternative<flutter::EncodableMap>(arguments)) {
      auto settings = std::get<flutter::EncodableMap>(arguments);
      if (settings.size() > 0) {
        try {
          ApplySettings(settings);
        } catch (const std::invalid_argument& ex) {
          LOG_ERROR("[Exception] %s\n", ex.what());
          result->Error(ex.what());
          return;
        }
      }
    }
    result->Success();
  } else if (method_name.compare("canGoBack") == 0) {
    result->Success(flutter::EncodableValue(webview_instance_->CanGoBack()));
  } else if (method_name.compare("canGoForward") == 0) {
    result->Success(flutter::EncodableValue(webview_instance_->CanGoForward()));
  } else if (method_name.compare("goBack") == 0) {
    webview_instance_->GoBack();
    result->Success();
  } else if (method_name.compare("goForward") == 0) {
    webview_instance_->GoForward();
    result->Success();
  } else if (method_name.compare("reload") == 0) {
    webview_instance_->Reload();
    result->Success();
  } else if (method_name.compare("currentUrl") == 0) {
    result->Success(flutter::EncodableValue(webview_instance_->GetURL()));
  } else if (method_name.compare("evaluateJavascript") == 0 ||
             method_name.compare("runJavascriptReturningResult") == 0 ||
             method_name.compare("runJavascript") == 0) {
    if (std::holds_alternative<std::string>(arguments)) {
      std::string js_string = std::get<std::string>(arguments);
      bool returns_value =
          method_name.compare("runJavascript") == 0 ? false : true;
      webview_instance_->EvaluateJavaScript(
          js_string,
          [res = result.release(), returns_value](std::string value) {
            LOG_DEBUG("value: %s\n", value.c_str());
            if (res) {
              if (returns_value) {
                res->Success(flutter::EncodableValue(value));
              } else {
                res->Success();
              }
              delete res;
            }
          });
    } else {
      result->Error("InvalidArguments", "Please set javascript string");
    }
  } else if (method_name.compare("addJavascriptChannels") == 0) {
    if (std::holds_alternative<flutter::EncodableList>(arguments)) {
      auto name_list = std::get<flutter::EncodableList>(arguments);
      for (size_t i = 0; i < name_list.size(); i++) {
        if (std::holds_alternative<std::string>(name_list[i])) {
          RegisterJavaScriptChannelName(std::get<std::string>(name_list[i]));
        }
      }
    }
    result->Success();
  } else if (method_name.compare("removeJavascriptChannels") == 0) {
    if (std::holds_alternative<flutter::EncodableList>(arguments)) {
      auto name_list = std::get<flutter::EncodableList>(arguments);
      for (size_t i = 0; i < name_list.size(); i++) {
        if (std::holds_alternative<std::string>(name_list[i])) {
          webview_instance_->RemoveJavascriptInterface(
              std::get<std::string>(name_list[i]), "postMessage");
        }
      }
    }
    result->Success();
  } else if (method_name.compare("clearCache") == 0) {
    webview_instance_->ClearCache();
    result->Success();
  } else if (method_name.compare("getTitle") == 0) {
    result->Success(flutter::EncodableValue(webview_instance_->GetTitle()));
  } else if (method_name.compare("scrollTo") == 0) {
    int x = 0, y = 0;
    if (GetValueFromEncodableMap(arguments, "x", &x) &&
        GetValueFromEncodableMap(arguments, "y", &y)) {
      webview_instance_->ScrollTo(x, y);
      result->Success();
      return;
    }
    result->Error("InvalidArguments", "Please set 'x' or 'y' properly");
  } else if (method_name.compare("scrollBy") == 0) {
    int x = 0, y = 0;
    if (GetValueFromEncodableMap(arguments, "x", &x) &&
        GetValueFromEncodableMap(arguments, "y", &y)) {
      webview_instance_->ScrollBy(x, y);
      result->Success();
      return;
    }
    result->Error("InvalidArguments", "Please set 'x' or 'y' properly");
  } else if (method_name.compare("getScrollX") == 0) {
    result->Success(flutter::EncodableValue(webview_instance_->GetScrollX()));
  } else if (method_name.compare("getScrollY") == 0) {
    result->Success(flutter::EncodableValue(webview_instance_->GetScrollY()));
  } else if (method_name.compare("loadFlutterAsset") == 0) {
    if (std::holds_alternative<std::string>(arguments)) {
      std::string key = std::get<std::string>(arguments);
      std::string path;
      char* resPath = app_get_resource_path();
      if (resPath) {
        path = std::string("file://") + resPath + "flutter_assets/" + key;
        free(resPath);
        webview_instance_->LoadURL(path);
        result->Success();
        return;
      }
    }
    result->Error("InvalidArguments", "Please set 'key' properly");
  } else if (method_name.compare("loadHtmlString") == 0) {
    std::string html;
    std::string baseUrl;
    if (!GetValueFromEncodableMap(arguments, "html", &html)) {
      result->Error("InvalidArguments", "Please set 'html' properly");
      return;
    }
    if (GetValueFromEncodableMap(arguments, "baseUrl", &baseUrl)) {
      LOG_DEBUG(
          "loadHtmlString : baseUrl is not yet supported. It will be "
          "ignored.\n ");
    }
    webview_instance_->LoadData(html);
    result->Success();
  } else if (method_name.compare("loadFile") == 0) {
    if (std::holds_alternative<std::string>(arguments)) {
      std::string absoluteFilePath =
          std::string("file://") + std::get<std::string>(arguments);
      webview_instance_->LoadURL(absoluteFilePath);
      result->Success();
      return;
    }
    result->Error("InvalidArguments", "Please set 'absoluteFilePath' properly");
  } else if (method_name.compare("loadRequest") == 0) {
    result->NotImplemented();
  } else if (method_name.compare("setCookie") == 0) {
    result->NotImplemented();
  } else {
    result->NotImplemented();
  }
}

void WebView::HandleCookieMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (webview_instance_ == nullptr) {
    result->Error("Not Webview created");
    return;
  }

  const auto method_name = method_call.method_name();

  LOG_DEBUG("WebView::HandleMethodCall : %s \n ", method_name.c_str());

  if (method_name.compare("clearCookies") == 0) {
    LWE::CookieManager* cookie = LWE::CookieManager::GetInstance();
    cookie->ClearCookies();
    result->Success(flutter::EncodableValue(true));
  } else {
    result->NotImplemented();
  }
}

FlutterDesktopGpuBuffer* WebView::ObtainGpuBuffer(size_t width, size_t height) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!candidate_surface_) {
    if (rendered_surface_) {
      return rendered_surface_->GpuBuffer();
    }
    return nullptr;
  }
  if (rendered_surface_ && rendered_surface_->IsUsed()) {
    tbm_pool_->Release(rendered_surface_);
  }
  rendered_surface_ = candidate_surface_;
  candidate_surface_ = nullptr;
  return rendered_surface_->GpuBuffer();
}

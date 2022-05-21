// Copyright 2020 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "video_player_tizen_plugin.h"

#include <app_common.h>
#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <string>

#include "flutter_texture_registrar.h"
#include "log.h"
#include "messages.h"
#include "video_player.h"
#include "video_player_error.h"
#include "video_player_options.h"

namespace {

class VideoPlayerTizenPlugin : public flutter::Plugin, public VideoPlayerApi {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar);

  // Creates a plugin that communicates on the given channel.
  VideoPlayerTizenPlugin(flutter::PluginRegistrar *registrar);
  virtual ~VideoPlayerTizenPlugin();

  virtual std::optional<FlutterError> initialize() override;
  virtual ErrorOr<std::unique_ptr<TextureMessage>> create(
      const CreateMessage &createMsg) override;
  virtual std::optional<FlutterError> dispose(
      const TextureMessage &textureMsg) override;
  virtual std::optional<FlutterError> setLooping(
      const LoopingMessage &loopingMsg) override;
  virtual std::optional<FlutterError> setVolume(
      const VolumeMessage &volumeMsg) override;
  virtual std::optional<FlutterError> setPlaybackSpeed(
      const PlaybackSpeedMessage &speedMsg) override;
  virtual std::optional<FlutterError> play(
      const TextureMessage &textureMsg) override;
  virtual std::optional<FlutterError> pause(
      const TextureMessage &textureMsg) override;
  virtual ErrorOr<std::unique_ptr<PositionMessage>> position(
      const TextureMessage &textureMsg) override;
  virtual std::optional<FlutterError> seekTo(
      const PositionMessage &positionMsg,
      const SeekCompletedCallback &onSeekCompleted) override;
  virtual std::optional<FlutterError> setMixWithOthers(
      const MixWithOthersMessage &mixWithOthersMsg) override;

 private:
  void DisposeAllPlayers();

  flutter::PluginRegistrar *plugin_registrar_;
  flutter::TextureRegistrar *texture_registrar_;
  VideoPlayerOptions options_;
  std::map<int64_t, std::unique_ptr<VideoPlayer>> players_;
};

void VideoPlayerTizenPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrar *registrar) {
  auto plugin = std::make_unique<VideoPlayerTizenPlugin>(registrar);
  registrar->AddPlugin(std::move(plugin));
}

VideoPlayerTizenPlugin::VideoPlayerTizenPlugin(
    flutter::PluginRegistrar *registrar)
    : plugin_registrar_(registrar) {
  texture_registrar_ = registrar->texture_registrar();

  VideoPlayerApi::SetUp(registrar->messenger(), this);
}

VideoPlayerTizenPlugin::~VideoPlayerTizenPlugin() { DisposeAllPlayers(); }

void VideoPlayerTizenPlugin::DisposeAllPlayers() {
  LOG_DEBUG("[VideoPlayerTizenPlugin] player count: %d", players_.size());

  for (const auto &[id, player] : players_) {
    player->Dispose();
  }
  players_.clear();
}

std::optional<FlutterError> VideoPlayerTizenPlugin::initialize() {
  LOG_DEBUG("[VideoPlayerTizenPlugin] initialize");

  DisposeAllPlayers();

  return FlutterError();
}

ErrorOr<std::unique_ptr<TextureMessage>> VideoPlayerTizenPlugin::create(
    const CreateMessage &createMsg) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] asset: %s", createMsg.getAsset().c_str());
  LOG_DEBUG("[VideoPlayerTizenPlugin] uri: %s", createMsg.getUri().c_str());
  LOG_DEBUG("[VideoPlayerTizenPlugin] packageName: %s",
            createMsg.getPackageName().c_str());
  LOG_DEBUG("[VideoPlayerTizenPlugin] formatHint: %s",
            createMsg.getFormatHint().c_str());

  std::string uri;
  if (createMsg.getAsset().empty()) {
    uri = createMsg.getUri();
  } else {
    char *res_path = app_get_resource_path();
    if (res_path) {
      uri = uri + res_path + "flutter_assets/" + createMsg.getAsset();
      free(res_path);
    } else {
      throw VideoPlayerError("Internal error", "Failed to get resource path.");
    }
  }
  LOG_DEBUG("[VideoPlayerTizenPlugin] player uri: %s", uri.c_str());

  auto player = std::make_unique<VideoPlayer>(
      plugin_registrar_, texture_registrar_, uri, options_);
  int64_t texture_id = player->GetTextureId();
  players_[texture_id] = std::move(player);

  auto result = std::make_unique<TextureMessage>();
  result->setTextureId(texture_id);
  return ErrorOr<TextureMessage>::MakeWithUniquePtr(std::move(result));
}

std::optional<FlutterError> VideoPlayerTizenPlugin::dispose(
    const TextureMessage &textureMsg) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] textureId: %ld",
            textureMsg.getTextureId());

  auto iter = players_.find(textureMsg.getTextureId());
  if (iter != players_.end()) {
    iter->second->Dispose();
    players_.erase(iter);
  }

  return FlutterError();
}

std::optional<FlutterError> VideoPlayerTizenPlugin::setLooping(
    const LoopingMessage &loopingMsg) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] textureId: %ld",
            loopingMsg.getTextureId());
  LOG_DEBUG("[VideoPlayerTizenPlugin] isLooping: %d",
            loopingMsg.getIsLooping());

  auto iter = players_.find(loopingMsg.getTextureId());
  if (iter != players_.end()) {
    iter->second->SetLooping(loopingMsg.getIsLooping());
  }

  return FlutterError();
}

std::optional<FlutterError> VideoPlayerTizenPlugin::setVolume(
    const VolumeMessage &volumeMsg) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] textureId: %ld",
            volumeMsg.getTextureId());
  LOG_DEBUG("[VideoPlayerTizenPlugin] volume: %f", volumeMsg.getVolume());

  auto iter = players_.find(volumeMsg.getTextureId());
  if (iter != players_.end()) {
    iter->second->SetVolume(volumeMsg.getVolume());
  }

  return FlutterError();
}

std::optional<FlutterError> VideoPlayerTizenPlugin::setPlaybackSpeed(
    const PlaybackSpeedMessage &speedMsg) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] textureId: %ld", speedMsg.getTextureId());
  LOG_DEBUG("[VideoPlayerTizenPlugin] speed: %f", speedMsg.getSpeed());

  auto iter = players_.find(speedMsg.getTextureId());
  if (iter != players_.end()) {
    iter->second->SetPlaybackSpeed(speedMsg.getSpeed());
  }

  return FlutterError();
}

std::optional<FlutterError> VideoPlayerTizenPlugin::play(
    const TextureMessage &textureMsg) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] textureId: %ld",
            textureMsg.getTextureId());

  auto iter = players_.find(textureMsg.getTextureId());
  if (iter != players_.end()) {
    iter->second->Play();
  }

  return FlutterError();
}

std::optional<FlutterError> VideoPlayerTizenPlugin::pause(
    const TextureMessage &textureMsg) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] textureId: %ld",
            textureMsg.getTextureId());

  auto iter = players_.find(textureMsg.getTextureId());
  if (iter != players_.end()) {
    iter->second->Pause();
  }

  return FlutterError();
}

ErrorOr<std::unique_ptr<PositionMessage>> VideoPlayerTizenPlugin::position(
    const TextureMessage &textureMsg) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] textureId: %ld",
            textureMsg.getTextureId());

  auto result = std::make_unique<PositionMessage>();
  auto iter = players_.find(textureMsg.getTextureId());
  if (iter != players_.end()) {
    result->setTextureId(textureMsg.getTextureId());
    result->setPosition(iter->second->GetPosition());
  }
  return ErrorOr<PositionMessage>::MakeWithUniquePtr(std::move(result));
}

std::optional<FlutterError> VideoPlayerTizenPlugin::seekTo(
    const PositionMessage &positionMsg,
    const SeekCompletedCallback &onSeekCompleted) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] textureId: %ld",
            positionMsg.getTextureId());
  LOG_DEBUG("[VideoPlayerTizenPlugin] position: %ld",
            positionMsg.getPosition());

  auto iter = players_.find(positionMsg.getTextureId());
  if (iter != players_.end()) {
    iter->second->SeekTo(positionMsg.getPosition(), onSeekCompleted);
  }

  return FlutterError();
}

std::optional<FlutterError> VideoPlayerTizenPlugin::setMixWithOthers(
    const MixWithOthersMessage &mixWithOthersMsg) {
  LOG_DEBUG("[VideoPlayerTizenPlugin] mixWithOthers: %d",
            mixWithOthersMsg.getMixWithOthers());

  options_.setMixWithOthers(mixWithOthersMsg.getMixWithOthers());

  return FlutterError();
}

}  // namespace

void VideoPlayerTizenPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  VideoPlayerTizenPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrar>(registrar));
}

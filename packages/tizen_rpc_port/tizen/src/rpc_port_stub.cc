// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rpc_port_stub.h"

#include <rpc-port-parcel.h>

#include <utility>
#include <vector>

#include "log.h"

namespace tizen {

namespace {

// flutter::EncodableMap CreateEncodableMap(const char* event, const char*
// sender,
//                                          const char* instance,
//                                          rpc_port_stub_h handle) {
//   return {{flutter::EncodableValue("event"),
//            flutter::EncodableValue(std::string(event))},
//           {flutter::EncodableValue("sender"),
//            flutter::EncodableValue(std::string(sender))},
//           {flutter::EncodableValue("instance"),
//            flutter::EncodableValue(std::string(instance))},
//           {flutter::EncodableValue("handle"),
//            flutter::EncodableValue(reinterpret_cast<int64_t>(handle))}};
// }

}  // namespace

RpcPortStub::RpcPortStub(const std::string& port_name) {
  rpc_port_stub_h handle = nullptr;

  int ret = rpc_port_stub_create(&handle, port_name.c_str());
  if (ret != RPC_PORT_ERROR_NONE) {
    LOG_ERROR("rpc_port_stub_create failed: %s", get_error_message(ret));
    return;
  }

  ret = rpc_port_stub_add_connected_event_cb(handle, OnConnectedEvent, this);
  if (ret != RPC_PORT_ERROR_NONE) {
    LOG_ERROR("rpc_port_stub_add_connected_event_cb failed: %s",
              get_error_message(ret));
    return;
  }
  ret = rpc_port_stub_add_disconnected_event_cb(handle, OnDisconnectedEvent,
                                                this);
  if (ret != RPC_PORT_ERROR_NONE) {
    LOG_ERROR("rpc_port_stub_add_disconnected_event_cb failed: %s",
              get_error_message(ret));
    return;
  }
  ret = rpc_port_stub_add_received_event_cb(handle, OnReceivedEvent, this);
  if (ret != RPC_PORT_ERROR_NONE) {
    LOG_ERROR("rpc_port_stub_add_received_event_cb failed: %s",
              get_error_message(ret));
    return;
  }

  handle_ = handle;
}

RpcPortStub::~RpcPortStub() {
  if (handle_) {
    rpc_port_stub_destroy(handle_);
  }
}

bool RpcPortStub::Listen() {
  int ret = rpc_port_stub_listen(handle_);
  if (ret != RPC_PORT_ERROR_NONE) {
    LOG_ERROR("rpc_port_stub_listen failed: %s", get_error_message(ret));
    return false;
  }
  return true;
}

void RpcPortStub::OnConnectedEvent(const char* sender, const char* instance,
                                   void* user_data) {
  LOG_DEBUG("Connected: sender(%s), instance(%s)", sender, instance);

  auto* self = static_cast<RpcPortStub*>(user_data);
  if (self->connect_callback_) {
    self->connect_callback_(sender, instance);
  }

  // rpc_port_stub_h handle = reinterpret_cast<rpc_port_stub_h>(user_data);
  // flutter::EncodableMap map =
  //     CreateEncodableMap("connected", sender, instance, handle);
  // event_sink_->Success(flutter::EncodableValue(map));
}

void RpcPortStub::OnDisconnectedEvent(const char* sender, const char* instance,
                                      void* user_data) {
  LOG_DEBUG("Disconnected: sender(%s), instance(%s)", sender, instance);

  auto* self = static_cast<RpcPortStub*>(user_data);
  if (self->disconnect_callback_) {
    self->disconnect_callback_(sender, instance);
  }

  // rpc_port_stub_h handle = reinterpret_cast<rpc_port_stub_h>(user_data);
  // flutter::EncodableMap map =
  //     CreateEncodableMap("disconnected", sender, instance, handle);
  // event_sink_->Success(flutter::EncodableValue(map));
}

int RpcPortStub::OnReceivedEvent(const char* sender, const char* instance,
                                 rpc_port_h port, void* user_data) {
  LOG_DEBUG("Received: sender(%s), instance(%s)", sender, instance);

  auto* self = static_cast<RpcPortStub*>(user_data);
  if (self->receive_callback_) {
    self->receive_callback_(sender, instance, port);
  }

  // rpc_port_parcel_h parcel = nullptr;
  // int ret = rpc_port_parcel_create_from_port(&parcel, port);
  // if (ret != RPC_PORT_ERROR_NONE) {
  //   event_sink_->Error("rpc_port_parcel_create_from_port failed",
  //                      std::string(get_error_message(ret)));
  //   return 0;
  // }

  // unsigned char* raw = nullptr;
  // unsigned int size = 0;
  // ret = rpc_port_parcel_get_raw(parcel, reinterpret_cast<void**>(&raw),
  // &size); if (ret != RPC_PORT_ERROR_NONE) {
  //   rpc_port_parcel_destroy(parcel);
  //   event_sink_->Error("rpc_port_parcel_get_raw failed",
  //                      std::string(get_error_message(ret)));
  //   return 0;
  // }
  // std::vector<uint8_t> raw_data(raw, raw + size);
  // rpc_port_parcel_destroy(parcel);

  // rpc_port_stub_h handle = reinterpret_cast<rpc_port_stub_h>(user_data);
  // flutter::EncodableMap map =
  //     CreateEncodableMap("received", sender, instance, handle);
  // map[flutter::EncodableValue("rawData")] =
  //     flutter::EncodableValue(std::move(raw_data));
  // event_sink_->Success(flutter::EncodableValue(map));

  return 0;
}

}  // namespace tizen

// Copyright 2022 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_PLUGIN_RPC_PORT_STUB_H_
#define FLUTTER_PLUGIN_RPC_PORT_STUB_H_

#include <rpc-port.h>

#include <functional>
#include <memory>
#include <string>

#include "rpc_port_result.h"

namespace tizen {

using OnStubConnectEvent =
    std::function<void(std::string sender, std::string instance)>;
using OnStubReceiveEvent = std::function<void(
    std::string sender, std::string instance, rpc_port_h port)>;

class RpcPortStub {
 public:
  explicit RpcPortStub(const std::string& port_name);
  ~RpcPortStub();

  bool Listen();

  void SetConnectHandler(OnStubConnectEvent on_connect) {
    connect_callback_ = on_connect;
  }

  void SetDisconnectHandler(OnStubConnectEvent on_disconnect) {
    disconnect_callback_ = on_disconnect;
  }

  void SetReceiveHandler(OnStubReceiveEvent on_receive) {
    receive_callback_ = on_receive;
  }

 private:
  static void OnConnectedEvent(const char* sender, const char* instance,
                               void* user_data);
  static void OnDisconnectedEvent(const char* sender, const char* instance,
                                  void* user_data);
  static int OnReceivedEvent(const char* sender, const char* instance,
                             rpc_port_h port, void* user_data);

  rpc_port_stub_h handle_ = nullptr;

  OnStubConnectEvent connect_callback_;
  OnStubConnectEvent disconnect_callback_;
  OnStubReceiveEvent receive_callback_;
};

}  // namespace tizen

#endif  // FLUTTER_PLUGIN_RPC_PORT_STUB_H_

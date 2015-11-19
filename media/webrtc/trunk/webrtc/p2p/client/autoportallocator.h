/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_CLIENT_AUTOPORTALLOCATOR_H_
#define WEBRTC_P2P_CLIENT_AUTOPORTALLOCATOR_H_

#include <string>
#include <vector>

#include "webrtc/p2p/client/httpportallocator.h"
#include "webrtc/base/sigslot.h"

// This class sets the relay and stun servers using XmppClient.
// It enables the client to traverse Proxy and NAT.
class AutoPortAllocator : public cricket::HttpPortAllocator {
 public:
  AutoPortAllocator(rtc::NetworkManager* network_manager,
                    const std::string& user_agent)
      : cricket::HttpPortAllocator(network_manager, user_agent) {
  }

  // Creates and initiates a task to get relay token from XmppClient and set
  // it appropriately.
  void SetXmppClient(buzz::XmppClient* client) {
    // The JingleInfoTask is freed by the task-runner.
    buzz::JingleInfoTask* jit = new buzz::JingleInfoTask(client);
    jit->SignalJingleInfo.connect(this, &AutoPortAllocator::OnJingleInfo);
    jit->Start();
    jit->RefreshJingleInfoNow();
  }

 private:
  void OnJingleInfo(
      const std::string& token,
      const std::vector<std::string>& relay_hosts,
      const std::vector<rtc::SocketAddress>& stun_hosts) {
    SetRelayToken(token);
    SetStunHosts(stun_hosts);
    SetRelayHosts(relay_hosts);
  }
};

#endif  // WEBRTC_P2P_CLIENT_AUTOPORTALLOCATOR_H_

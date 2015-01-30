/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_FAKENETWORK_H_
#define WEBRTC_BASE_FAKENETWORK_H_

#include <string>
#include <vector>

#include "webrtc/base/network.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/thread.h"

namespace rtc {

const int kFakeIPv4NetworkPrefixLength = 24;
const int kFakeIPv6NetworkPrefixLength = 64;

// Fake network manager that allows us to manually specify the IPs to use.
class FakeNetworkManager : public NetworkManagerBase,
                           public MessageHandler {
 public:
  FakeNetworkManager()
      : thread_(Thread::Current()),
        next_index_(0),
        started_(false),
        sent_first_update_(false) {
  }

  typedef std::vector<SocketAddress> IfaceList;

  void AddInterface(const SocketAddress& iface) {
    // ensure a unique name for the interface
    SocketAddress address("test" + rtc::ToString(next_index_++), 0);
    address.SetResolvedIP(iface.ipaddr());
    ifaces_.push_back(address);
    DoUpdateNetworks();
  }

  void RemoveInterface(const SocketAddress& iface) {
    for (IfaceList::iterator it = ifaces_.begin();
         it != ifaces_.end(); ++it) {
      if (it->EqualIPs(iface)) {
        ifaces_.erase(it);
        break;
      }
    }
    DoUpdateNetworks();
  }

  virtual void StartUpdating() {
    if (started_) {
      if (sent_first_update_)
        SignalNetworksChanged();
      return;
    }

    started_ = true;
    sent_first_update_ = false;
    thread_->Post(this);
  }

  virtual void StopUpdating() {
    started_ = false;
  }

  // MessageHandler interface.
  virtual void OnMessage(Message* msg) {
    DoUpdateNetworks();
  }

 private:
  void DoUpdateNetworks() {
    if (!started_)
      return;
    std::vector<Network*> networks;
    for (IfaceList::iterator it = ifaces_.begin();
         it != ifaces_.end(); ++it) {
      int prefix_length = 0;
      if (it->ipaddr().family() == AF_INET) {
        prefix_length = kFakeIPv4NetworkPrefixLength;
      } else if (it->ipaddr().family() == AF_INET6) {
        prefix_length = kFakeIPv6NetworkPrefixLength;
      }
      IPAddress prefix = TruncateIP(it->ipaddr(), prefix_length);
      scoped_ptr<Network> net(new Network(it->hostname(),
                                          it->hostname(),
                                          prefix,
                                          prefix_length));
      net->AddIP(it->ipaddr());
      networks.push_back(net.release());
    }
    bool changed;
    MergeNetworkList(networks, &changed);
    if (changed || !sent_first_update_) {
      SignalNetworksChanged();
      sent_first_update_ = true;
    }
  }

  Thread* thread_;
  IfaceList ifaces_;
  int next_index_;
  bool started_;
  bool sent_first_update_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_FAKENETWORK_H_

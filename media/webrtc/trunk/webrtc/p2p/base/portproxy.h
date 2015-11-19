/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_PORTPROXY_H_
#define WEBRTC_P2P_BASE_PORTPROXY_H_

#include "webrtc/p2p/base/portinterface.h"
#include "webrtc/base/sigslot.h"

namespace rtc {
class Network;
}

namespace cricket {

class PortProxy : public PortInterface, public sigslot::has_slots<> {
 public:
  PortProxy() {}
  virtual ~PortProxy() {}

  PortInterface* impl() { return impl_; }
  void set_impl(PortInterface* port);

  virtual const std::string& Type() const;
  virtual rtc::Network* Network() const;

  virtual void SetIceProtocolType(IceProtocolType protocol);
  virtual IceProtocolType IceProtocol() const;

  // Methods to set/get ICE role and tiebreaker values.
  virtual void SetIceRole(IceRole role);
  virtual IceRole GetIceRole() const;

  virtual void SetIceTiebreaker(uint64 tiebreaker);
  virtual uint64 IceTiebreaker() const;

  virtual bool SharedSocket() const;

  // Forwards call to the actual Port.
  virtual void PrepareAddress();
  virtual Connection* CreateConnection(const Candidate& remote_candidate,
                                       CandidateOrigin origin);
  virtual Connection* GetConnection(
      const rtc::SocketAddress& remote_addr);

  virtual int SendTo(const void* data, size_t size,
                     const rtc::SocketAddress& addr,
                     const rtc::PacketOptions& options,
                     bool payload);
  virtual int SetOption(rtc::Socket::Option opt, int value);
  virtual int GetOption(rtc::Socket::Option opt, int* value);
  virtual int GetError();

  virtual const std::vector<Candidate>& Candidates() const;

  virtual void SendBindingResponse(StunMessage* request,
                                   const rtc::SocketAddress& addr);
  virtual void SendBindingErrorResponse(
        StunMessage* request, const rtc::SocketAddress& addr,
        int error_code, const std::string& reason);

  virtual void EnablePortPackets();
  virtual std::string ToString() const;

 private:
  void OnUnknownAddress(PortInterface *port,
                        const rtc::SocketAddress &addr,
                        ProtocolType proto,
                        IceMessage *stun_msg,
                        const std::string &remote_username,
                        bool port_muxed);
  void OnRoleConflict(PortInterface* port);
  void OnPortDestroyed(PortInterface* port);

  PortInterface* impl_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_PORTPROXY_H_

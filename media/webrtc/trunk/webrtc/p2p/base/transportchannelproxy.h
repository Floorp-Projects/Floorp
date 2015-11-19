/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_TRANSPORTCHANNELPROXY_H_
#define WEBRTC_P2P_BASE_TRANSPORTCHANNELPROXY_H_

#include <string>
#include <utility>
#include <vector>

#include "webrtc/p2p/base/transportchannel.h"
#include "webrtc/base/messagehandler.h"

namespace rtc {
class Thread;
}

namespace cricket {

class TransportChannelImpl;

// Proxies calls between the client and the transport channel implementation.
// This is needed because clients are allowed to create channels before the
// network negotiation is complete.  Hence, we create a proxy up front, and
// when negotiation completes, connect the proxy to the implementaiton.
class TransportChannelProxy : public TransportChannel,
                              public rtc::MessageHandler {
 public:
  TransportChannelProxy(const std::string& content_name,
                        int component);
  virtual ~TransportChannelProxy();

  TransportChannelImpl* impl() { return impl_; }

  virtual TransportChannelState GetState() const;

  // Sets the implementation to which we will proxy.
  void SetImplementation(TransportChannelImpl* impl);

  // Implementation of the TransportChannel interface.  These simply forward to
  // the implementation.
  virtual int SendPacket(const char* data, size_t len,
                         const rtc::PacketOptions& options,
                         int flags);
  virtual int SetOption(rtc::Socket::Option opt, int value);
  virtual bool GetOption(rtc::Socket::Option opt, int* value);
  virtual int GetError();
  virtual IceRole GetIceRole() const;
  virtual bool GetStats(ConnectionInfos* infos);
  virtual bool IsDtlsActive() const;
  virtual bool GetSslRole(rtc::SSLRole* role) const;
  virtual bool SetSslRole(rtc::SSLRole role);
  virtual bool SetSrtpCiphers(const std::vector<std::string>& ciphers);
  virtual bool GetSrtpCipher(std::string* cipher);
  virtual bool GetSslCipher(std::string* cipher);
  virtual bool GetLocalIdentity(rtc::SSLIdentity** identity) const;
  virtual bool GetRemoteCertificate(rtc::SSLCertificate** cert) const;
  virtual bool ExportKeyingMaterial(const std::string& label,
                            const uint8* context,
                            size_t context_len,
                            bool use_context,
                            uint8* result,
                            size_t result_len);

 private:
  // Catch signals from the implementation channel.  These just forward to the
  // client (after updating our state to match).
  void OnReadableState(TransportChannel* channel);
  void OnWritableState(TransportChannel* channel);
  void OnReadPacket(TransportChannel* channel, const char* data, size_t size,
                    const rtc::PacketTime& packet_time, int flags);
  void OnReadyToSend(TransportChannel* channel);
  void OnRouteChange(TransportChannel* channel, const Candidate& candidate);

  void OnMessage(rtc::Message* message);

  typedef std::pair<rtc::Socket::Option, int> OptionPair;
  typedef std::vector<OptionPair> OptionList;
  rtc::Thread* worker_thread_;
  TransportChannelImpl* impl_;
  OptionList options_;
  std::vector<std::string> pending_srtp_ciphers_;

  DISALLOW_EVIL_CONSTRUCTORS(TransportChannelProxy);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TRANSPORTCHANNELPROXY_H_

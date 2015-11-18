/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/p2p/base/transportchannelproxy.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"

namespace cricket {

enum {
  MSG_UPDATESTATE,
};

TransportChannelProxy::TransportChannelProxy(const std::string& content_name,
                                             int component)
    : TransportChannel(content_name, component),
      impl_(NULL) {
  worker_thread_ = rtc::Thread::Current();
}

TransportChannelProxy::~TransportChannelProxy() {
  // Clearing any pending signal.
  worker_thread_->Clear(this);
  if (impl_) {
    impl_->GetTransport()->DestroyChannel(impl_->component());
  }
}

void TransportChannelProxy::SetImplementation(TransportChannelImpl* impl) {
  ASSERT(rtc::Thread::Current() == worker_thread_);

  if (impl == impl_) {
    // Ignore if the |impl| has already been set.
    LOG(LS_WARNING) << "Ignored TransportChannelProxy::SetImplementation call "
                    << "with a same impl as the existing one.";
    return;
  }

  // Destroy any existing impl_.
  if (impl_) {
    impl_->GetTransport()->DestroyChannel(impl_->component());
  }

  // Adopt the supplied impl, and connect to its signals.
  impl_ = impl;

  if (impl_) {
    impl_->SignalReadableState.connect(
        this, &TransportChannelProxy::OnReadableState);
    impl_->SignalWritableState.connect(
        this, &TransportChannelProxy::OnWritableState);
    impl_->SignalReadPacket.connect(
        this, &TransportChannelProxy::OnReadPacket);
    impl_->SignalReadyToSend.connect(
        this, &TransportChannelProxy::OnReadyToSend);
    impl_->SignalRouteChange.connect(
        this, &TransportChannelProxy::OnRouteChange);
    for (const auto& pair : options_) {
      impl_->SetOption(pair.first, pair.second);
    }

    // Push down the SRTP ciphers, if any were set.
    if (!pending_srtp_ciphers_.empty()) {
      impl_->SetSrtpCiphers(pending_srtp_ciphers_);
    }
  }

  // Post ourselves a message to see if we need to fire state callbacks.
  worker_thread_->Post(this, MSG_UPDATESTATE);
}

int TransportChannelProxy::SendPacket(const char* data, size_t len,
                                      const rtc::PacketOptions& options,
                                      int flags) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  // Fail if we don't have an impl yet.
  if (!impl_) {
    return -1;
  }
  return impl_->SendPacket(data, len, options, flags);
}

int TransportChannelProxy::SetOption(rtc::Socket::Option opt, int value) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  options_.push_back(OptionPair(opt, value));
  if (!impl_) {
    return 0;
  }
  return impl_->SetOption(opt, value);
}

bool TransportChannelProxy::GetOption(rtc::Socket::Option opt, int* value) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (impl_) {
    return impl_->GetOption(opt, value);
  }

  for (const auto& pair : options_) {
    if (pair.first == opt) {
      *value = pair.second;
      return true;
    }
  }
  return false;
}

int TransportChannelProxy::GetError() {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return 0;
  }
  return impl_->GetError();
}

TransportChannelState TransportChannelProxy::GetState() const {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return TransportChannelState::STATE_CONNECTING;
  }
  return impl_->GetState();
}

bool TransportChannelProxy::GetStats(ConnectionInfos* infos) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return false;
  }
  return impl_->GetStats(infos);
}

bool TransportChannelProxy::IsDtlsActive() const {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return false;
  }
  return impl_->IsDtlsActive();
}

bool TransportChannelProxy::GetSslRole(rtc::SSLRole* role) const {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return false;
  }
  return impl_->GetSslRole(role);
}

bool TransportChannelProxy::SetSslRole(rtc::SSLRole role) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return false;
  }
  return impl_->SetSslRole(role);
}

bool TransportChannelProxy::SetSrtpCiphers(const std::vector<std::string>&
                                           ciphers) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  pending_srtp_ciphers_ = ciphers;  // Cache so we can send later, but always
                                    // set so it stays consistent.
  if (impl_) {
    return impl_->SetSrtpCiphers(ciphers);
  }
  return true;
}

bool TransportChannelProxy::GetSrtpCipher(std::string* cipher) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return false;
  }
  return impl_->GetSrtpCipher(cipher);
}

bool TransportChannelProxy::GetSslCipher(std::string* cipher) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return false;
  }
  return impl_->GetSslCipher(cipher);
}

bool TransportChannelProxy::GetLocalIdentity(
    rtc::SSLIdentity** identity) const {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return false;
  }
  return impl_->GetLocalIdentity(identity);
}

bool TransportChannelProxy::GetRemoteCertificate(
    rtc::SSLCertificate** cert) const {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return false;
  }
  return impl_->GetRemoteCertificate(cert);
}

bool TransportChannelProxy::ExportKeyingMaterial(const std::string& label,
                                                 const uint8* context,
                                                 size_t context_len,
                                                 bool use_context,
                                                 uint8* result,
                                                 size_t result_len) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return false;
  }
  return impl_->ExportKeyingMaterial(label, context, context_len, use_context,
                                     result, result_len);
}

IceRole TransportChannelProxy::GetIceRole() const {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (!impl_) {
    return ICEROLE_UNKNOWN;
  }
  return impl_->GetIceRole();
}

void TransportChannelProxy::OnReadableState(TransportChannel* channel) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == impl_);
  set_readable(impl_->readable());
  // Note: SignalReadableState fired by set_readable.
}

void TransportChannelProxy::OnWritableState(TransportChannel* channel) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == impl_);
  set_writable(impl_->writable());
  // Note: SignalWritableState fired by set_readable.
}

void TransportChannelProxy::OnReadPacket(
    TransportChannel* channel, const char* data, size_t size,
    const rtc::PacketTime& packet_time, int flags) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == impl_);
  SignalReadPacket(this, data, size, packet_time, flags);
}

void TransportChannelProxy::OnReadyToSend(TransportChannel* channel) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == impl_);
  SignalReadyToSend(this);
}

void TransportChannelProxy::OnRouteChange(TransportChannel* channel,
                                          const Candidate& candidate) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == impl_);
  SignalRouteChange(this, candidate);
}

void TransportChannelProxy::OnMessage(rtc::Message* msg) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  if (msg->message_id == MSG_UPDATESTATE) {
     // If impl_ is already readable or writable, push up those signals.
     set_readable(impl_ ? impl_->readable() : false);
     set_writable(impl_ ? impl_->writable() : false);
  }
}

}  // namespace cricket

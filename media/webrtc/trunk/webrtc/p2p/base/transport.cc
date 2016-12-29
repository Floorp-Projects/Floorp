/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>  // for std::pair

#include "webrtc/p2p/base/transport.h"

#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/port.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace cricket {

static bool VerifyIceParams(const TransportDescription& desc) {
  // For legacy protocols.
  if (desc.ice_ufrag.empty() && desc.ice_pwd.empty())
    return true;

  if (desc.ice_ufrag.length() < ICE_UFRAG_MIN_LENGTH ||
      desc.ice_ufrag.length() > ICE_UFRAG_MAX_LENGTH) {
    return false;
  }
  if (desc.ice_pwd.length() < ICE_PWD_MIN_LENGTH ||
      desc.ice_pwd.length() > ICE_PWD_MAX_LENGTH) {
    return false;
  }
  return true;
}

bool BadTransportDescription(const std::string& desc, std::string* err_desc) {
  if (err_desc) {
    *err_desc = desc;
  }
  LOG(LS_ERROR) << desc;
  return false;
}

bool IceCredentialsChanged(const std::string& old_ufrag,
                           const std::string& old_pwd,
                           const std::string& new_ufrag,
                           const std::string& new_pwd) {
  // TODO(jiayl): The standard (RFC 5245 Section 9.1.1.1) says that ICE should
  // restart when both the ufrag and password are changed, but we do restart
  // when either ufrag or passwrod is changed to keep compatible with GICE. We
  // should clean this up when GICE is no longer used.
  return (old_ufrag != new_ufrag) || (old_pwd != new_pwd);
}

static bool IceCredentialsChanged(const TransportDescription& old_desc,
                                  const TransportDescription& new_desc) {
  return IceCredentialsChanged(old_desc.ice_ufrag, old_desc.ice_pwd,
                               new_desc.ice_ufrag, new_desc.ice_pwd);
}

Transport::Transport(const std::string& name, PortAllocator* allocator)
    : name_(name), allocator_(allocator) {}

Transport::~Transport() {
  RTC_DCHECK(channels_destroyed_);
}

void Transport::SetIceRole(IceRole role) {
  ice_role_ = role;
  for (const auto& kv : channels_) {
    kv.second->SetIceRole(ice_role_);
  }
}

bool Transport::GetRemoteSSLCertificate(rtc::SSLCertificate** cert) {
  if (channels_.empty()) {
    return false;
  }

  auto iter = channels_.begin();
  return iter->second->GetRemoteSSLCertificate(cert);
}

void Transport::SetIceConfig(const IceConfig& config) {
  ice_config_ = config;
  for (const auto& kv : channels_) {
    kv.second->SetIceConfig(ice_config_);
  }
}

bool Transport::SetLocalTransportDescription(
    const TransportDescription& description,
    ContentAction action,
    std::string* error_desc) {
  bool ret = true;

  if (!VerifyIceParams(description)) {
    return BadTransportDescription("Invalid ice-ufrag or ice-pwd length",
                                   error_desc);
  }

  if (local_description_ &&
      IceCredentialsChanged(*local_description_, description)) {
    IceRole new_ice_role =
        (action == CA_OFFER) ? ICEROLE_CONTROLLING : ICEROLE_CONTROLLED;

    // It must be called before ApplyLocalTransportDescription, which may
    // trigger an ICE restart and depends on the new ICE role.
    SetIceRole(new_ice_role);
  }

  local_description_.reset(new TransportDescription(description));

  for (const auto& kv : channels_) {
    ret &= ApplyLocalTransportDescription(kv.second, error_desc);
  }
  if (!ret) {
    return false;
  }

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (action == CA_PRANSWER || action == CA_ANSWER) {
    ret &= NegotiateTransportDescription(action, error_desc);
  }
  if (ret) {
    local_description_set_ = true;
    ConnectChannels();
  }

  return ret;
}

bool Transport::SetRemoteTransportDescription(
    const TransportDescription& description,
    ContentAction action,
    std::string* error_desc) {
  bool ret = true;

  if (!VerifyIceParams(description)) {
    return BadTransportDescription("Invalid ice-ufrag or ice-pwd length",
                                   error_desc);
  }

  remote_description_.reset(new TransportDescription(description));
  for (const auto& kv : channels_) {
    ret &= ApplyRemoteTransportDescription(kv.second, error_desc);
  }

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (action == CA_PRANSWER || action == CA_ANSWER) {
    ret = NegotiateTransportDescription(CA_OFFER, error_desc);
  }
  if (ret) {
    remote_description_set_ = true;
  }

  return ret;
}

TransportChannelImpl* Transport::CreateChannel(int component) {
  TransportChannelImpl* channel;

  // Create the entry if it does not exist.
  bool channel_exists = false;
  auto iter = channels_.find(component);
  if (iter == channels_.end()) {
    channel = CreateTransportChannel(component);
    channels_.insert(std::pair<int, TransportChannelImpl*>(component, channel));
  } else {
    channel = iter->second;
    channel_exists = true;
  }

  channels_destroyed_ = false;

  if (channel_exists) {
    // If this is an existing channel, we should just return it.
    return channel;
  }

  // Push down our transport state to the new channel.
  channel->SetIceRole(ice_role_);
  channel->SetIceTiebreaker(tiebreaker_);
  channel->SetIceConfig(ice_config_);
  // TODO(ronghuawu): Change CreateChannel to be able to return error since
  // below Apply**Description calls can fail.
  if (local_description_)
    ApplyLocalTransportDescription(channel, nullptr);
  if (remote_description_)
    ApplyRemoteTransportDescription(channel, nullptr);
  if (local_description_ && remote_description_)
    ApplyNegotiatedTransportDescription(channel, nullptr);

  if (connect_requested_) {
    channel->Connect();
  }
  return channel;
}

TransportChannelImpl* Transport::GetChannel(int component) {
  auto iter = channels_.find(component);
  return (iter != channels_.end()) ? iter->second : nullptr;
}

bool Transport::HasChannels() {
  return !channels_.empty();
}

void Transport::DestroyChannel(int component) {
  auto iter = channels_.find(component);
  if (iter == channels_.end())
    return;

  TransportChannelImpl* channel = iter->second;
  channels_.erase(iter);
  DestroyTransportChannel(channel);
}

void Transport::ConnectChannels() {
  if (connect_requested_ || channels_.empty())
    return;

  connect_requested_ = true;

  if (!local_description_) {
    // TOOD(mallinath) : TransportDescription(TD) shouldn't be generated here.
    // As Transport must know TD is offer or answer and cricket::Transport
    // doesn't have the capability to decide it. This should be set by the
    // Session.
    // Session must generate local TD before remote candidates pushed when
    // initiate request initiated by the remote.
    LOG(LS_INFO) << "Transport::ConnectChannels: No local description has "
                 << "been set. Will generate one.";
    TransportDescription desc(
        std::vector<std::string>(), rtc::CreateRandomString(ICE_UFRAG_LENGTH),
        rtc::CreateRandomString(ICE_PWD_LENGTH), ICEMODE_FULL,
        CONNECTIONROLE_NONE, nullptr, Candidates());
    SetLocalTransportDescription(desc, CA_OFFER, nullptr);
  }

  CallChannels(&TransportChannelImpl::Connect);
}

void Transport::MaybeStartGathering() {
  if (connect_requested_) {
    CallChannels(&TransportChannelImpl::MaybeStartGathering);
  }
}

void Transport::DestroyAllChannels() {
  for (const auto& kv : channels_) {
    DestroyTransportChannel(kv.second);
  }
  channels_.clear();
  channels_destroyed_ = true;
}

void Transport::CallChannels(TransportChannelFunc func) {
  for (const auto& kv : channels_) {
    (kv.second->*func)();
  }
}

bool Transport::VerifyCandidate(const Candidate& cand, std::string* error) {
  // No address zero.
  if (cand.address().IsNil() || cand.address().IsAnyIP()) {
    *error = "candidate has address of zero";
    return false;
  }

  // Disallow all ports below 1024, except for 80 and 443 on public addresses.
  int port = cand.address().port();
  if (cand.protocol() == TCP_PROTOCOL_NAME &&
      (cand.tcptype() == TCPTYPE_ACTIVE_STR || port == 0)) {
    // Expected for active-only candidates per
    // http://tools.ietf.org/html/rfc6544#section-4.5 so no error.
    // Libjingle clients emit port 0, in "active" mode.
    return true;
  }
  if (port < 1024) {
    if ((port != 80) && (port != 443)) {
      *error = "candidate has port below 1024, but not 80 or 443";
      return false;
    }

    if (cand.address().IsPrivateIP()) {
      *error = "candidate has port of 80 or 443 with private IP address";
      return false;
    }
  }

  return true;
}


bool Transport::GetStats(TransportStats* stats) {
  stats->transport_name = name();
  stats->channel_stats.clear();
  for (auto kv : channels_) {
    TransportChannelImpl* channel = kv.second;
    TransportChannelStats substats;
    substats.component = channel->component();
    channel->GetSrtpCryptoSuite(&substats.srtp_crypto_suite);
    channel->GetSslCipherSuite(&substats.ssl_cipher_suite);
    if (!channel->GetStats(&substats.connection_infos)) {
      return false;
    }
    stats->channel_stats.push_back(substats);
  }
  return true;
}

bool Transport::AddRemoteCandidates(const std::vector<Candidate>& candidates,
                                    std::string* error) {
  ASSERT(!channels_destroyed_);
  // Verify each candidate before passing down to transport layer.
  for (const Candidate& cand : candidates) {
    if (!VerifyCandidate(cand, error)) {
      return false;
    }
    if (!HasChannel(cand.component())) {
      *error = "Candidate has unknown component: " + cand.ToString() +
               " for content: " + name();
      return false;
    }
  }

  for (const Candidate& candidate : candidates) {
    TransportChannelImpl* channel = GetChannel(candidate.component());
    if (channel != nullptr) {
      channel->AddRemoteCandidate(candidate);
    }
  }
  return true;
}

bool Transport::ApplyLocalTransportDescription(TransportChannelImpl* ch,
                                               std::string* error_desc) {
  ch->SetIceCredentials(local_description_->ice_ufrag,
                        local_description_->ice_pwd);
  return true;
}

bool Transport::ApplyRemoteTransportDescription(TransportChannelImpl* ch,
                                                std::string* error_desc) {
  ch->SetRemoteIceCredentials(remote_description_->ice_ufrag,
                              remote_description_->ice_pwd);
  return true;
}

bool Transport::ApplyNegotiatedTransportDescription(
    TransportChannelImpl* channel,
    std::string* error_desc) {
  channel->SetRemoteIceMode(remote_ice_mode_);
  return true;
}

bool Transport::NegotiateTransportDescription(ContentAction local_role,
                                              std::string* error_desc) {
  // TODO(ekr@rtfm.com): This is ICE-specific stuff. Refactor into
  // P2PTransport.

  // If transport is in ICEROLE_CONTROLLED and remote end point supports only
  // ice_lite, this local end point should take CONTROLLING role.
  if (ice_role_ == ICEROLE_CONTROLLED &&
      remote_description_->ice_mode == ICEMODE_LITE) {
    SetIceRole(ICEROLE_CONTROLLING);
  }

  // Update remote ice_mode to all existing channels.
  remote_ice_mode_ = remote_description_->ice_mode;

  // Now that we have negotiated everything, push it downward.
  // Note that we cache the result so that if we have race conditions
  // between future SetRemote/SetLocal invocations and new channel
  // creation, we have the negotiation state saved until a new
  // negotiation happens.
  for (const auto& kv : channels_) {
    if (!ApplyNegotiatedTransportDescription(kv.second, error_desc)) {
      return false;
    }
  }
  return true;
}

}  // namespace cricket

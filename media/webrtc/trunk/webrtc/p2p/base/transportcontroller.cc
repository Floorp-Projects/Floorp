/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/transportcontroller.h"

#include <algorithm>

#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/dtlstransport.h"
#include "webrtc/p2p/base/p2ptransport.h"
#include "webrtc/p2p/base/port.h"

namespace cricket {

enum {
  MSG_ICECONNECTIONSTATE,
  MSG_RECEIVING,
  MSG_ICEGATHERINGSTATE,
  MSG_CANDIDATESGATHERED,
};

struct CandidatesData : public rtc::MessageData {
  CandidatesData(const std::string& transport_name,
                 const Candidates& candidates)
      : transport_name(transport_name), candidates(candidates) {}

  std::string transport_name;
  Candidates candidates;
};

TransportController::TransportController(rtc::Thread* signaling_thread,
                                         rtc::Thread* worker_thread,
                                         PortAllocator* port_allocator)
    : signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      port_allocator_(port_allocator) {}

TransportController::~TransportController() {
  worker_thread_->Invoke<void>(
      rtc::Bind(&TransportController::DestroyAllTransports_w, this));
  signaling_thread_->Clear(this);
}

bool TransportController::SetSslMaxProtocolVersion(
    rtc::SSLProtocolVersion version) {
  return worker_thread_->Invoke<bool>(rtc::Bind(
      &TransportController::SetSslMaxProtocolVersion_w, this, version));
}

void TransportController::SetIceConfig(const IceConfig& config) {
  worker_thread_->Invoke<void>(
      rtc::Bind(&TransportController::SetIceConfig_w, this, config));
}

void TransportController::SetIceRole(IceRole ice_role) {
  worker_thread_->Invoke<void>(
      rtc::Bind(&TransportController::SetIceRole_w, this, ice_role));
}

bool TransportController::GetSslRole(const std::string& transport_name,
                                     rtc::SSLRole* role) {
  return worker_thread_->Invoke<bool>(rtc::Bind(
      &TransportController::GetSslRole_w, this, transport_name, role));
}

bool TransportController::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  return worker_thread_->Invoke<bool>(rtc::Bind(
      &TransportController::SetLocalCertificate_w, this, certificate));
}

bool TransportController::GetLocalCertificate(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
  return worker_thread_->Invoke<bool>(
      rtc::Bind(&TransportController::GetLocalCertificate_w, this,
                transport_name, certificate));
}

bool TransportController::GetRemoteSSLCertificate(
    const std::string& transport_name,
    rtc::SSLCertificate** cert) {
  return worker_thread_->Invoke<bool>(
      rtc::Bind(&TransportController::GetRemoteSSLCertificate_w, this,
                transport_name, cert));
}

bool TransportController::SetLocalTransportDescription(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  return worker_thread_->Invoke<bool>(
      rtc::Bind(&TransportController::SetLocalTransportDescription_w, this,
                transport_name, tdesc, action, err));
}

bool TransportController::SetRemoteTransportDescription(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  return worker_thread_->Invoke<bool>(
      rtc::Bind(&TransportController::SetRemoteTransportDescription_w, this,
                transport_name, tdesc, action, err));
}

void TransportController::MaybeStartGathering() {
  worker_thread_->Invoke<void>(
      rtc::Bind(&TransportController::MaybeStartGathering_w, this));
}

bool TransportController::AddRemoteCandidates(const std::string& transport_name,
                                              const Candidates& candidates,
                                              std::string* err) {
  return worker_thread_->Invoke<bool>(
      rtc::Bind(&TransportController::AddRemoteCandidates_w, this,
                transport_name, candidates, err));
}

bool TransportController::ReadyForRemoteCandidates(
    const std::string& transport_name) {
  return worker_thread_->Invoke<bool>(rtc::Bind(
      &TransportController::ReadyForRemoteCandidates_w, this, transport_name));
}

bool TransportController::GetStats(const std::string& transport_name,
                                   TransportStats* stats) {
  return worker_thread_->Invoke<bool>(
      rtc::Bind(&TransportController::GetStats_w, this, transport_name, stats));
}

TransportChannel* TransportController::CreateTransportChannel_w(
    const std::string& transport_name,
    int component) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  auto it = FindChannel_w(transport_name, component);
  if (it != channels_.end()) {
    // Channel already exists; increment reference count and return.
    it->AddRef();
    return it->get();
  }

  // Need to create a new channel.
  Transport* transport = GetOrCreateTransport_w(transport_name);
  TransportChannelImpl* channel = transport->CreateChannel(component);
  channel->SignalWritableState.connect(
      this, &TransportController::OnChannelWritableState_w);
  channel->SignalReceivingState.connect(
      this, &TransportController::OnChannelReceivingState_w);
  channel->SignalGatheringState.connect(
      this, &TransportController::OnChannelGatheringState_w);
  channel->SignalCandidateGathered.connect(
      this, &TransportController::OnChannelCandidateGathered_w);
  channel->SignalRoleConflict.connect(
      this, &TransportController::OnChannelRoleConflict_w);
  channel->SignalConnectionRemoved.connect(
      this, &TransportController::OnChannelConnectionRemoved_w);
  channels_.insert(channels_.end(), RefCountedChannel(channel))->AddRef();
  // Adding a channel could cause aggregate state to change.
  UpdateAggregateStates_w();
  return channel;
}

void TransportController::DestroyTransportChannel_w(
    const std::string& transport_name,
    int component) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  auto it = FindChannel_w(transport_name, component);
  if (it == channels_.end()) {
    LOG(LS_WARNING) << "Attempting to delete " << transport_name
                    << " TransportChannel " << component
                    << ", which doesn't exist.";
    return;
  }

  it->DecRef();
  if (it->ref() > 0) {
    return;
  }

  channels_.erase(it);
  Transport* transport = GetTransport_w(transport_name);
  transport->DestroyChannel(component);
  // Just as we create a Transport when its first channel is created,
  // we delete it when its last channel is deleted.
  if (!transport->HasChannels()) {
    DestroyTransport_w(transport_name);
  }
  // Removing a channel could cause aggregate state to change.
  UpdateAggregateStates_w();
}

const rtc::scoped_refptr<rtc::RTCCertificate>&
TransportController::certificate_for_testing() {
  return certificate_;
}

Transport* TransportController::CreateTransport_w(
    const std::string& transport_name) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  Transport* transport = new DtlsTransport<P2PTransport>(
      transport_name, port_allocator(), certificate_);
  return transport;
}

Transport* TransportController::GetTransport_w(
    const std::string& transport_name) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  auto iter = transports_.find(transport_name);
  return (iter != transports_.end()) ? iter->second : nullptr;
}

void TransportController::OnMessage(rtc::Message* pmsg) {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  switch (pmsg->message_id) {
    case MSG_ICECONNECTIONSTATE: {
      rtc::TypedMessageData<IceConnectionState>* data =
          static_cast<rtc::TypedMessageData<IceConnectionState>*>(pmsg->pdata);
      SignalConnectionState(data->data());
      delete data;
      break;
    }
    case MSG_RECEIVING: {
      rtc::TypedMessageData<bool>* data =
          static_cast<rtc::TypedMessageData<bool>*>(pmsg->pdata);
      SignalReceiving(data->data());
      delete data;
      break;
    }
    case MSG_ICEGATHERINGSTATE: {
      rtc::TypedMessageData<IceGatheringState>* data =
          static_cast<rtc::TypedMessageData<IceGatheringState>*>(pmsg->pdata);
      SignalGatheringState(data->data());
      delete data;
      break;
    }
    case MSG_CANDIDATESGATHERED: {
      CandidatesData* data = static_cast<CandidatesData*>(pmsg->pdata);
      SignalCandidatesGathered(data->transport_name, data->candidates);
      delete data;
      break;
    }
    default:
      ASSERT(false);
  }
}

std::vector<TransportController::RefCountedChannel>::iterator
TransportController::FindChannel_w(const std::string& transport_name,
                                   int component) {
  return std::find_if(
      channels_.begin(), channels_.end(),
      [transport_name, component](const RefCountedChannel& channel) {
        return channel->transport_name() == transport_name &&
               channel->component() == component;
      });
}

Transport* TransportController::GetOrCreateTransport_w(
    const std::string& transport_name) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  Transport* transport = GetTransport_w(transport_name);
  if (transport) {
    return transport;
  }

  transport = CreateTransport_w(transport_name);
  // The stuff below happens outside of CreateTransport_w so that unit tests
  // can override CreateTransport_w to return a different type of transport.
  transport->SetSslMaxProtocolVersion(ssl_max_version_);
  transport->SetIceConfig(ice_config_);
  transport->SetIceRole(ice_role_);
  transport->SetIceTiebreaker(ice_tiebreaker_);
  if (certificate_) {
    transport->SetLocalCertificate(certificate_);
  }
  transports_[transport_name] = transport;

  return transport;
}

void TransportController::DestroyTransport_w(
    const std::string& transport_name) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  auto iter = transports_.find(transport_name);
  if (iter != transports_.end()) {
    delete iter->second;
    transports_.erase(transport_name);
  }
}

void TransportController::DestroyAllTransports_w() {
  RTC_DCHECK(worker_thread_->IsCurrent());

  for (const auto& kv : transports_) {
    delete kv.second;
  }
  transports_.clear();
}

bool TransportController::SetSslMaxProtocolVersion_w(
    rtc::SSLProtocolVersion version) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  // Max SSL version can only be set before transports are created.
  if (!transports_.empty()) {
    return false;
  }

  ssl_max_version_ = version;
  return true;
}

void TransportController::SetIceConfig_w(const IceConfig& config) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  ice_config_ = config;
  for (const auto& kv : transports_) {
    kv.second->SetIceConfig(ice_config_);
  }
}

void TransportController::SetIceRole_w(IceRole ice_role) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  ice_role_ = ice_role;
  for (const auto& kv : transports_) {
    kv.second->SetIceRole(ice_role_);
  }
}

bool TransportController::GetSslRole_w(const std::string& transport_name,
                                       rtc::SSLRole* role) {
  RTC_DCHECK(worker_thread()->IsCurrent());

  Transport* t = GetTransport_w(transport_name);
  if (!t) {
    return false;
  }

  return t->GetSslRole(role);
}

bool TransportController::SetLocalCertificate_w(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  if (certificate_) {
    return false;
  }
  if (!certificate) {
    return false;
  }
  certificate_ = certificate;

  for (const auto& kv : transports_) {
    kv.second->SetLocalCertificate(certificate_);
  }
  return true;
}

bool TransportController::GetLocalCertificate_w(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  Transport* t = GetTransport_w(transport_name);
  if (!t) {
    return false;
  }

  return t->GetLocalCertificate(certificate);
}

bool TransportController::GetRemoteSSLCertificate_w(
    const std::string& transport_name,
    rtc::SSLCertificate** cert) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  Transport* t = GetTransport_w(transport_name);
  if (!t) {
    return false;
  }

  return t->GetRemoteSSLCertificate(cert);
}

bool TransportController::SetLocalTransportDescription_w(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  RTC_DCHECK(worker_thread()->IsCurrent());

  Transport* transport = GetTransport_w(transport_name);
  if (!transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    // TODO(deadbeef): Make callers smarter so they won't attempt to set a
    // description on a deleted transport.
    return true;
  }

  return transport->SetLocalTransportDescription(tdesc, action, err);
}

bool TransportController::SetRemoteTransportDescription_w(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  RTC_DCHECK(worker_thread()->IsCurrent());

  Transport* transport = GetTransport_w(transport_name);
  if (!transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    // TODO(deadbeef): Make callers smarter so they won't attempt to set a
    // description on a deleted transport.
    return true;
  }

  return transport->SetRemoteTransportDescription(tdesc, action, err);
}

void TransportController::MaybeStartGathering_w() {
  for (const auto& kv : transports_) {
    kv.second->MaybeStartGathering();
  }
}

bool TransportController::AddRemoteCandidates_w(
    const std::string& transport_name,
    const Candidates& candidates,
    std::string* err) {
  RTC_DCHECK(worker_thread()->IsCurrent());

  Transport* transport = GetTransport_w(transport_name);
  if (!transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    return true;
  }

  return transport->AddRemoteCandidates(candidates, err);
}

bool TransportController::ReadyForRemoteCandidates_w(
    const std::string& transport_name) {
  RTC_DCHECK(worker_thread()->IsCurrent());

  Transport* transport = GetTransport_w(transport_name);
  if (!transport) {
    return false;
  }
  return transport->ready_for_remote_candidates();
}

bool TransportController::GetStats_w(const std::string& transport_name,
                                     TransportStats* stats) {
  RTC_DCHECK(worker_thread()->IsCurrent());

  Transport* transport = GetTransport_w(transport_name);
  if (!transport) {
    return false;
  }
  return transport->GetStats(stats);
}

void TransportController::OnChannelWritableState_w(TransportChannel* channel) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  LOG(LS_INFO) << channel->transport_name() << " TransportChannel "
               << channel->component() << " writability changed to "
               << channel->writable() << ".";
  UpdateAggregateStates_w();
}

void TransportController::OnChannelReceivingState_w(TransportChannel* channel) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  UpdateAggregateStates_w();
}

void TransportController::OnChannelGatheringState_w(
    TransportChannelImpl* channel) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  UpdateAggregateStates_w();
}

void TransportController::OnChannelCandidateGathered_w(
    TransportChannelImpl* channel,
    const Candidate& candidate) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  // We should never signal peer-reflexive candidates.
  if (candidate.type() == PRFLX_PORT_TYPE) {
    RTC_DCHECK(false);
    return;
  }
  std::vector<Candidate> candidates;
  candidates.push_back(candidate);
  CandidatesData* data =
      new CandidatesData(channel->transport_name(), candidates);
  signaling_thread_->Post(this, MSG_CANDIDATESGATHERED, data);
}

void TransportController::OnChannelRoleConflict_w(
    TransportChannelImpl* channel) {
  RTC_DCHECK(worker_thread_->IsCurrent());

  if (ice_role_switch_) {
    LOG(LS_WARNING)
        << "Repeat of role conflict signal from TransportChannelImpl.";
    return;
  }

  ice_role_switch_ = true;
  IceRole reversed_role = (ice_role_ == ICEROLE_CONTROLLING)
                              ? ICEROLE_CONTROLLED
                              : ICEROLE_CONTROLLING;
  for (const auto& kv : transports_) {
    kv.second->SetIceRole(reversed_role);
  }
}

void TransportController::OnChannelConnectionRemoved_w(
    TransportChannelImpl* channel) {
  RTC_DCHECK(worker_thread_->IsCurrent());
  LOG(LS_INFO) << channel->transport_name() << " TransportChannel "
               << channel->component()
               << " connection removed. Check if state is complete.";
  UpdateAggregateStates_w();
}

void TransportController::UpdateAggregateStates_w() {
  RTC_DCHECK(worker_thread_->IsCurrent());

  IceConnectionState new_connection_state = kIceConnectionConnecting;
  IceGatheringState new_gathering_state = kIceGatheringNew;
  bool any_receiving = false;
  bool any_failed = false;
  bool all_connected = !channels_.empty();
  bool all_completed = !channels_.empty();
  bool any_gathering = false;
  bool all_done_gathering = !channels_.empty();
  for (const auto& channel : channels_) {
    any_receiving = any_receiving || channel->receiving();
    any_failed = any_failed ||
                 channel->GetState() == TransportChannelState::STATE_FAILED;
    all_connected = all_connected && channel->writable();
    all_completed =
        all_completed && channel->writable() &&
        channel->GetState() == TransportChannelState::STATE_COMPLETED &&
        channel->GetIceRole() == ICEROLE_CONTROLLING &&
        channel->gathering_state() == kIceGatheringComplete;
    any_gathering =
        any_gathering || channel->gathering_state() != kIceGatheringNew;
    all_done_gathering = all_done_gathering &&
                         channel->gathering_state() == kIceGatheringComplete;
  }

  if (any_failed) {
    new_connection_state = kIceConnectionFailed;
  } else if (all_completed) {
    new_connection_state = kIceConnectionCompleted;
  } else if (all_connected) {
    new_connection_state = kIceConnectionConnected;
  }
  if (connection_state_ != new_connection_state) {
    connection_state_ = new_connection_state;
    signaling_thread_->Post(
        this, MSG_ICECONNECTIONSTATE,
        new rtc::TypedMessageData<IceConnectionState>(new_connection_state));
  }

  if (receiving_ != any_receiving) {
    receiving_ = any_receiving;
    signaling_thread_->Post(this, MSG_RECEIVING,
                            new rtc::TypedMessageData<bool>(any_receiving));
  }

  if (all_done_gathering) {
    new_gathering_state = kIceGatheringComplete;
  } else if (any_gathering) {
    new_gathering_state = kIceGatheringGathering;
  }
  if (gathering_state_ != new_gathering_state) {
    gathering_state_ = new_gathering_state;
    signaling_thread_->Post(
        this, MSG_ICEGATHERINGSTATE,
        new rtc::TypedMessageData<IceGatheringState>(new_gathering_state));
  }
}

}  // namespace cricket

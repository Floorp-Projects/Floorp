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

#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/port.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"

namespace cricket {

using rtc::Bind;

enum {
  MSG_ONSIGNALINGREADY = 1,
  MSG_ONREMOTECANDIDATE,
  MSG_READSTATE,
  MSG_WRITESTATE,
  MSG_REQUESTSIGNALING,
  MSG_CANDIDATEREADY,
  MSG_ROUTECHANGE,
  MSG_CONNECTING,
  MSG_CANDIDATEALLOCATIONCOMPLETE,
  MSG_ROLECONFLICT,
  MSG_COMPLETED,
  MSG_FAILED,
};

struct ChannelParams : public rtc::MessageData {
  ChannelParams() : channel(NULL), candidate(NULL) {}
  explicit ChannelParams(int component)
      : component(component), channel(NULL), candidate(NULL) {}
  explicit ChannelParams(Candidate* candidate)
      : channel(NULL), candidate(candidate) {
  }

  ~ChannelParams() {
    delete candidate;
  }

  std::string name;
  int component;
  TransportChannelImpl* channel;
  Candidate* candidate;
};

static std::string IceProtoToString(TransportProtocol proto) {
  std::string proto_str;
  switch (proto) {
    case ICEPROTO_GOOGLE:
      proto_str = "gice";
      break;
    case ICEPROTO_HYBRID:
      proto_str = "hybrid";
      break;
    case ICEPROTO_RFC5245:
      proto_str = "ice";
      break;
    default:
      ASSERT(false);
      break;
  }
  return proto_str;
}

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

Transport::Transport(rtc::Thread* signaling_thread,
                     rtc::Thread* worker_thread,
                     const std::string& content_name,
                     const std::string& type,
                     PortAllocator* allocator)
  : signaling_thread_(signaling_thread),
    worker_thread_(worker_thread),
    content_name_(content_name),
    type_(type),
    allocator_(allocator),
    destroyed_(false),
    readable_(TRANSPORT_STATE_NONE),
    writable_(TRANSPORT_STATE_NONE),
    was_writable_(false),
    connect_requested_(false),
    ice_role_(ICEROLE_UNKNOWN),
    tiebreaker_(0),
    protocol_(ICEPROTO_HYBRID),
    remote_ice_mode_(ICEMODE_FULL) {
}

Transport::~Transport() {
  ASSERT(signaling_thread_->IsCurrent());
  ASSERT(destroyed_);
}

void Transport::SetIceRole(IceRole role) {
  worker_thread_->Invoke<void>(Bind(&Transport::SetIceRole_w, this, role));
}

void Transport::SetIdentity(rtc::SSLIdentity* identity) {
  worker_thread_->Invoke<void>(Bind(&Transport::SetIdentity_w, this, identity));
}

bool Transport::GetIdentity(rtc::SSLIdentity** identity) {
  // The identity is set on the worker thread, so for safety it must also be
  // acquired on the worker thread.
  return worker_thread_->Invoke<bool>(
      Bind(&Transport::GetIdentity_w, this, identity));
}

bool Transport::GetRemoteCertificate(rtc::SSLCertificate** cert) {
  // Channels can be deleted on the worker thread, so for safety the remote
  // certificate is acquired on the worker thread.
  return worker_thread_->Invoke<bool>(
      Bind(&Transport::GetRemoteCertificate_w, this, cert));
}

bool Transport::GetRemoteCertificate_w(rtc::SSLCertificate** cert) {
  ASSERT(worker_thread()->IsCurrent());
  if (channels_.empty())
    return false;

  ChannelMap::iterator iter = channels_.begin();
  return iter->second->GetRemoteCertificate(cert);
}

bool Transport::SetLocalTransportDescription(
    const TransportDescription& description,
    ContentAction action,
    std::string* error_desc) {
  return worker_thread_->Invoke<bool>(Bind(
      &Transport::SetLocalTransportDescription_w, this,
      description, action, error_desc));
}

bool Transport::SetRemoteTransportDescription(
    const TransportDescription& description,
    ContentAction action,
    std::string* error_desc) {
  return worker_thread_->Invoke<bool>(Bind(
      &Transport::SetRemoteTransportDescription_w, this,
      description, action, error_desc));
}

TransportChannelImpl* Transport::CreateChannel(int component) {
  return worker_thread_->Invoke<TransportChannelImpl*>(Bind(
      &Transport::CreateChannel_w, this, component));
}

TransportChannelImpl* Transport::CreateChannel_w(int component) {
  ASSERT(worker_thread()->IsCurrent());
  TransportChannelImpl* impl;
  // TODO(tommi): We don't really need to grab the lock until the actual call
  // to insert() below and presumably hold it throughout initialization of
  // |impl| after the impl_exists check.  Maybe we can factor that out to
  // a separate function and not grab the lock in this function.
  // Actually, we probably don't need to hold the lock while initializing
  // |impl| since we can just do the insert when that's done.
  rtc::CritScope cs(&crit_);

  // Create the entry if it does not exist.
  bool impl_exists = false;
  auto iterator = channels_.find(component);
  if (iterator == channels_.end()) {
    impl = CreateTransportChannel(component);
    iterator = channels_.insert(std::pair<int, ChannelMapEntry>(
        component, ChannelMapEntry(impl))).first;
  } else {
    impl = iterator->second.get();
    impl_exists = true;
  }

  // Increase the ref count.
  iterator->second.AddRef();
  destroyed_ = false;

  if (impl_exists) {
    // If this is an existing channel, we should just return it without
    // connecting to all the signal again.
    return impl;
  }

  // Push down our transport state to the new channel.
  impl->SetIceRole(ice_role_);
  impl->SetIceTiebreaker(tiebreaker_);
  // TODO(ronghuawu): Change CreateChannel_w to be able to return error since
  // below Apply**Description_w calls can fail.
  if (local_description_)
    ApplyLocalTransportDescription_w(impl, NULL);
  if (remote_description_)
    ApplyRemoteTransportDescription_w(impl, NULL);
  if (local_description_ && remote_description_)
    ApplyNegotiatedTransportDescription_w(impl, NULL);

  impl->SignalReadableState.connect(this, &Transport::OnChannelReadableState);
  impl->SignalWritableState.connect(this, &Transport::OnChannelWritableState);
  impl->SignalRequestSignaling.connect(
      this, &Transport::OnChannelRequestSignaling);
  impl->SignalCandidateReady.connect(this, &Transport::OnChannelCandidateReady);
  impl->SignalRouteChange.connect(this, &Transport::OnChannelRouteChange);
  impl->SignalCandidatesAllocationDone.connect(
      this, &Transport::OnChannelCandidatesAllocationDone);
  impl->SignalRoleConflict.connect(this, &Transport::OnRoleConflict);
  impl->SignalConnectionRemoved.connect(
      this, &Transport::OnChannelConnectionRemoved);

  if (connect_requested_) {
    impl->Connect();
    if (channels_.size() == 1) {
      // If this is the first channel, then indicate that we have started
      // connecting.
      signaling_thread()->Post(this, MSG_CONNECTING, NULL);
    }
  }
  return impl;
}

TransportChannelImpl* Transport::GetChannel(int component) {
  // TODO(tommi,pthatcher): Since we're returning a pointer from the channels_
  // map, shouldn't we assume that we're on the worker thread? (The pointer
  // will be used outside of the lock).
  // And if we're on the worker thread, which is the only thread that modifies
  // channels_, can we skip grabbing the lock?
  rtc::CritScope cs(&crit_);
  ChannelMap::iterator iter = channels_.find(component);
  return (iter != channels_.end()) ? iter->second.get() : NULL;
}

bool Transport::HasChannels() {
  rtc::CritScope cs(&crit_);
  return !channels_.empty();
}

void Transport::DestroyChannel(int component) {
  worker_thread_->Invoke<void>(Bind(
      &Transport::DestroyChannel_w, this, component));
}

void Transport::DestroyChannel_w(int component) {
  ASSERT(worker_thread()->IsCurrent());

  ChannelMap::iterator iter = channels_.find(component);
  if (iter == channels_.end())
    return;

  TransportChannelImpl* impl = NULL;

  iter->second.DecRef();
  if (!iter->second.ref()) {
    impl = iter->second.get();
    rtc::CritScope cs(&crit_);
    channels_.erase(iter);
  }

  if (connect_requested_ && channels_.empty()) {
    // We're no longer attempting to connect.
    signaling_thread()->Post(this, MSG_CONNECTING, NULL);
  }

  if (impl) {
    // Check in case the deleted channel was the only non-writable channel.
    OnChannelWritableState(impl);
    DestroyTransportChannel(impl);
  }
}

void Transport::ConnectChannels() {
  ASSERT(signaling_thread()->IsCurrent());
  worker_thread_->Invoke<void>(Bind(&Transport::ConnectChannels_w, this));
}

void Transport::ConnectChannels_w() {
  ASSERT(worker_thread()->IsCurrent());
  if (connect_requested_ || channels_.empty())
    return;

  connect_requested_ = true;
  signaling_thread()->Post(this, MSG_CANDIDATEREADY, NULL);

  if (!local_description_) {
    // TOOD(mallinath) : TransportDescription(TD) shouldn't be generated here.
    // As Transport must know TD is offer or answer and cricket::Transport
    // doesn't have the capability to decide it. This should be set by the
    // Session.
    // Session must generate local TD before remote candidates pushed when
    // initiate request initiated by the remote.
    LOG(LS_INFO) << "Transport::ConnectChannels_w: No local description has "
                 << "been set. Will generate one.";
    TransportDescription desc(NS_GINGLE_P2P, std::vector<std::string>(),
                              rtc::CreateRandomString(ICE_UFRAG_LENGTH),
                              rtc::CreateRandomString(ICE_PWD_LENGTH),
                              ICEMODE_FULL, CONNECTIONROLE_NONE, NULL,
                              Candidates());
    SetLocalTransportDescription_w(desc, CA_OFFER, NULL);
  }

  CallChannels_w(&TransportChannelImpl::Connect);
  if (!channels_.empty()) {
    signaling_thread()->Post(this, MSG_CONNECTING, NULL);
  }
}

void Transport::OnConnecting_s() {
  ASSERT(signaling_thread()->IsCurrent());
  SignalConnecting(this);
}

void Transport::DestroyAllChannels() {
  ASSERT(signaling_thread()->IsCurrent());
  worker_thread_->Invoke<void>(Bind(&Transport::DestroyAllChannels_w, this));
  worker_thread()->Clear(this);
  signaling_thread()->Clear(this);
  destroyed_ = true;
}

void Transport::DestroyAllChannels_w() {
  ASSERT(worker_thread()->IsCurrent());

  std::vector<TransportChannelImpl*> impls;
  for (auto& iter : channels_) {
    iter.second.DecRef();
    if (!iter.second.ref())
      impls.push_back(iter.second.get());
  }

  {
    rtc::CritScope cs(&crit_);
    channels_.clear();
  }

  for (size_t i = 0; i < impls.size(); ++i)
    DestroyTransportChannel(impls[i]);
}

void Transport::ResetChannels() {
  ASSERT(signaling_thread()->IsCurrent());
  worker_thread_->Invoke<void>(Bind(&Transport::ResetChannels_w, this));
}

void Transport::ResetChannels_w() {
  ASSERT(worker_thread()->IsCurrent());

  // We are no longer attempting to connect
  connect_requested_ = false;

  // Clear out the old messages, they aren't relevant
  rtc::CritScope cs(&crit_);
  ready_candidates_.clear();

  // Reset all of the channels
  CallChannels_w(&TransportChannelImpl::Reset);
}

void Transport::OnSignalingReady() {
  ASSERT(signaling_thread()->IsCurrent());
  if (destroyed_) return;

  worker_thread()->Post(this, MSG_ONSIGNALINGREADY, NULL);

  // Notify the subclass.
  OnTransportSignalingReady();
}

void Transport::CallChannels_w(TransportChannelFunc func) {
  ASSERT(worker_thread()->IsCurrent());
  for (const auto& iter : channels_) {
    ((iter.second.get())->*func)();
  }
}

bool Transport::VerifyCandidate(const Candidate& cand, std::string* error) {
  // No address zero.
  if (cand.address().IsNil() || cand.address().IsAny()) {
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
  ASSERT(signaling_thread()->IsCurrent());
  return worker_thread_->Invoke<bool>(Bind(
      &Transport::GetStats_w, this, stats));
}

bool Transport::GetStats_w(TransportStats* stats) {
  ASSERT(worker_thread()->IsCurrent());
  stats->content_name = content_name();
  stats->channel_stats.clear();
  for (auto iter : channels_) {
    ChannelMapEntry& entry = iter.second;
    TransportChannelStats substats;
    substats.component = entry->component();
    entry->GetSrtpCipher(&substats.srtp_cipher);
    entry->GetSslCipher(&substats.ssl_cipher);
    if (!entry->GetStats(&substats.connection_infos)) {
      return false;
    }
    stats->channel_stats.push_back(substats);
  }
  return true;
}

bool Transport::GetSslRole(rtc::SSLRole* ssl_role) const {
  return worker_thread_->Invoke<bool>(Bind(
      &Transport::GetSslRole_w, this, ssl_role));
}

void Transport::OnRemoteCandidates(const std::vector<Candidate>& candidates) {
  for (std::vector<Candidate>::const_iterator iter = candidates.begin();
       iter != candidates.end();
       ++iter) {
    OnRemoteCandidate(*iter);
  }
}

void Transport::OnRemoteCandidate(const Candidate& candidate) {
  ASSERT(signaling_thread()->IsCurrent());
  if (destroyed_) return;

  if (!HasChannel(candidate.component())) {
    LOG(LS_WARNING) << "Ignoring candidate for unknown component "
                    << candidate.component();
    return;
  }

  ChannelParams* params = new ChannelParams(new Candidate(candidate));
  worker_thread()->Post(this, MSG_ONREMOTECANDIDATE, params);
}

void Transport::OnRemoteCandidate_w(const Candidate& candidate) {
  ASSERT(worker_thread()->IsCurrent());
  ChannelMap::iterator iter = channels_.find(candidate.component());
  // It's ok for a channel to go away while this message is in transit.
  if (iter != channels_.end()) {
    iter->second->OnCandidate(candidate);
  }
}

void Transport::OnChannelReadableState(TransportChannel* channel) {
  ASSERT(worker_thread()->IsCurrent());
  signaling_thread()->Post(this, MSG_READSTATE, NULL);
}

void Transport::OnChannelReadableState_s() {
  ASSERT(signaling_thread()->IsCurrent());
  TransportState readable = GetTransportState_s(true);
  if (readable_ != readable) {
    readable_ = readable;
    SignalReadableState(this);
  }
}

void Transport::OnChannelWritableState(TransportChannel* channel) {
  ASSERT(worker_thread()->IsCurrent());
  signaling_thread()->Post(this, MSG_WRITESTATE, NULL);

  MaybeCompleted_w();
}

void Transport::OnChannelWritableState_s() {
  ASSERT(signaling_thread()->IsCurrent());
  TransportState writable = GetTransportState_s(false);
  if (writable_ != writable) {
    was_writable_ = (writable_ == TRANSPORT_STATE_ALL);
    writable_ = writable;
    SignalWritableState(this);
  }
}

TransportState Transport::GetTransportState_s(bool read) {
  ASSERT(signaling_thread()->IsCurrent());

  rtc::CritScope cs(&crit_);
  bool any = false;
  bool all = !channels_.empty();
  for (const auto iter : channels_) {
    bool b = (read ? iter.second->readable() :
                     iter.second->writable());
    any |= b;
    all &=  b;
  }

  if (all) {
    return TRANSPORT_STATE_ALL;
  } else if (any) {
    return TRANSPORT_STATE_SOME;
  }

  return TRANSPORT_STATE_NONE;
}

void Transport::OnChannelRequestSignaling(TransportChannelImpl* channel) {
  ASSERT(worker_thread()->IsCurrent());
  // Resetting ICE state for the channel.
  ChannelMap::iterator iter = channels_.find(channel->component());
  if (iter != channels_.end())
    iter->second.set_candidates_allocated(false);
  signaling_thread()->Post(this, MSG_REQUESTSIGNALING, nullptr);
}

void Transport::OnChannelRequestSignaling_s() {
  ASSERT(signaling_thread()->IsCurrent());
  LOG(LS_INFO) << "Transport: " << content_name_ << ", allocating candidates";
  SignalRequestSignaling(this);
}

void Transport::OnChannelCandidateReady(TransportChannelImpl* channel,
                                        const Candidate& candidate) {
  ASSERT(worker_thread()->IsCurrent());
  rtc::CritScope cs(&crit_);
  ready_candidates_.push_back(candidate);

  // We hold any messages until the client lets us connect.
  if (connect_requested_) {
    signaling_thread()->Post(
        this, MSG_CANDIDATEREADY, NULL);
  }
}

void Transport::OnChannelCandidateReady_s() {
  ASSERT(signaling_thread()->IsCurrent());
  ASSERT(connect_requested_);

  std::vector<Candidate> candidates;
  {
    rtc::CritScope cs(&crit_);
    candidates.swap(ready_candidates_);
  }

  // we do the deleting of Candidate* here to keep the new above and
  // delete below close to each other
  if (!candidates.empty()) {
    SignalCandidatesReady(this, candidates);
  }
}

void Transport::OnChannelRouteChange(TransportChannel* channel,
                                     const Candidate& remote_candidate) {
  ASSERT(worker_thread()->IsCurrent());
  ChannelParams* params = new ChannelParams(new Candidate(remote_candidate));
  params->channel = static_cast<cricket::TransportChannelImpl*>(channel);
  signaling_thread()->Post(this, MSG_ROUTECHANGE, params);
}

void Transport::OnChannelRouteChange_s(const TransportChannel* channel,
                                       const Candidate& remote_candidate) {
  ASSERT(signaling_thread()->IsCurrent());
  SignalRouteChange(this, remote_candidate.component(), remote_candidate);
}

void Transport::OnChannelCandidatesAllocationDone(
    TransportChannelImpl* channel) {
  ASSERT(worker_thread()->IsCurrent());
  ChannelMap::iterator iter = channels_.find(channel->component());
  ASSERT(iter != channels_.end());
  LOG(LS_INFO) << "Transport: " << content_name_ << ", component "
               << channel->component() << " allocation complete";

  iter->second.set_candidates_allocated(true);

  // If all channels belonging to this Transport got signal, then
  // forward this signal to upper layer.
  // Can this signal arrive before all transport channels are created?
  for (auto& iter : channels_) {
    if (!iter.second.candidates_allocated())
      return;
  }
  signaling_thread_->Post(this, MSG_CANDIDATEALLOCATIONCOMPLETE);

  MaybeCompleted_w();
}

void Transport::OnChannelCandidatesAllocationDone_s() {
  ASSERT(signaling_thread()->IsCurrent());
  LOG(LS_INFO) << "Transport: " << content_name_ << " allocation complete";
  SignalCandidatesAllocationDone(this);
}

void Transport::OnRoleConflict(TransportChannelImpl* channel) {
  signaling_thread_->Post(this, MSG_ROLECONFLICT);
}

void Transport::OnChannelConnectionRemoved(TransportChannelImpl* channel) {
  ASSERT(worker_thread()->IsCurrent());
  MaybeCompleted_w();

  // Check if the state is now Failed.
  // Failed is only available in the Controlling ICE role.
  if (channel->GetIceRole() != ICEROLE_CONTROLLING) {
    return;
  }

  ChannelMap::iterator iter = channels_.find(channel->component());
  ASSERT(iter != channels_.end());
  // Failed can only occur after candidate allocation has stopped.
  if (!iter->second.candidates_allocated()) {
    return;
  }

  if (channel->GetState() == TransportChannelState::STATE_FAILED) {
    // A Transport has failed if any of its channels have no remaining
    // connections.
    signaling_thread_->Post(this, MSG_FAILED);
  }
}

void Transport::MaybeCompleted_w() {
  ASSERT(worker_thread()->IsCurrent());

  // When there is no channel created yet, calling this function could fire an
  // IceConnectionCompleted event prematurely.
  if (channels_.empty()) {
    return;
  }

  // A Transport's ICE process is completed if all of its channels are writable,
  // have finished allocating candidates, and have pruned all but one of their
  // connections.
  for (const auto& iter : channels_) {
    const TransportChannelImpl* channel = iter.second.get();
    if (!(channel->writable() &&
          channel->GetState() == TransportChannelState::STATE_COMPLETED &&
          channel->GetIceRole() == ICEROLE_CONTROLLING &&
          iter.second.candidates_allocated())) {
      return;
    }
  }

  signaling_thread_->Post(this, MSG_COMPLETED);
}

void Transport::SetIceRole_w(IceRole role) {
  ASSERT(worker_thread()->IsCurrent());
  rtc::CritScope cs(&crit_);
  ice_role_ = role;
  for (auto& iter : channels_) {
    iter.second->SetIceRole(ice_role_);
  }
}

void Transport::SetRemoteIceMode_w(IceMode mode) {
  ASSERT(worker_thread()->IsCurrent());
  remote_ice_mode_ = mode;
  // Shouldn't channels be created after this method executed?
  for (auto& iter : channels_) {
    iter.second->SetRemoteIceMode(remote_ice_mode_);
  }
}

bool Transport::SetLocalTransportDescription_w(
    const TransportDescription& desc,
    ContentAction action,
    std::string* error_desc) {
  ASSERT(worker_thread()->IsCurrent());
  bool ret = true;

  if (!VerifyIceParams(desc)) {
    return BadTransportDescription("Invalid ice-ufrag or ice-pwd length",
                                   error_desc);
  }

  // TODO(tommi,pthatcher): I'm not sure why we need to grab this lock at this
  // point. |local_description_| seems to always be modified on the worker
  // thread, so we should be able to use it here without grabbing the lock.
  // However, we _might_ need it before the call to reset() below?
  // Raw access to |local_description_| is granted to derived transports outside
  // of locking (see local_description() in the header file).
  // The contract is that the derived implementations must be aware of when the
  // description might change and do appropriate synchronization.
  rtc::CritScope cs(&crit_);
  if (local_description_ && IceCredentialsChanged(*local_description_, desc)) {
    IceRole new_ice_role = (action == CA_OFFER) ? ICEROLE_CONTROLLING
                                                : ICEROLE_CONTROLLED;

    // It must be called before ApplyLocalTransportDescription_w, which may
    // trigger an ICE restart and depends on the new ICE role.
    SetIceRole_w(new_ice_role);
  }

  local_description_.reset(new TransportDescription(desc));

  for (auto& iter : channels_) {
    ret &= ApplyLocalTransportDescription_w(iter.second.get(), error_desc);
  }
  if (!ret)
    return false;

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (action == CA_PRANSWER || action == CA_ANSWER) {
    ret &= NegotiateTransportDescription_w(action, error_desc);
  }
  return ret;
}

bool Transport::SetRemoteTransportDescription_w(
    const TransportDescription& desc,
    ContentAction action,
    std::string* error_desc) {
  bool ret = true;

  if (!VerifyIceParams(desc)) {
    return BadTransportDescription("Invalid ice-ufrag or ice-pwd length",
                                   error_desc);
  }

  // TODO(tommi,pthatcher): See todo for local_description_ above.
  rtc::CritScope cs(&crit_);
  remote_description_.reset(new TransportDescription(desc));
  for (auto& iter : channels_) {
    ret &= ApplyRemoteTransportDescription_w(iter.second.get(), error_desc);
  }

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (action == CA_PRANSWER || action == CA_ANSWER) {
    ret = NegotiateTransportDescription_w(CA_OFFER, error_desc);
  }
  return ret;
}

bool Transport::ApplyLocalTransportDescription_w(TransportChannelImpl* ch,
                                                 std::string* error_desc) {
  ASSERT(worker_thread()->IsCurrent());
  // If existing protocol_type is HYBRID, we may have not chosen the final
  // protocol type, so update the channel protocol type from the
  // local description. Otherwise, skip updating the protocol type.
  // We check for HYBRID to avoid accidental changes; in the case of a
  // session renegotiation, the new offer will have the google-ice ICE option,
  // so we need to make sure we don't switch back from ICE mode to HYBRID
  // when this happens.
  // There are some other ways we could have solved this, but this is the
  // simplest. The ultimate solution will be to get rid of GICE altogether.
  IceProtocolType protocol_type;
  if (ch->GetIceProtocolType(&protocol_type) &&
      protocol_type == ICEPROTO_HYBRID) {
    ch->SetIceProtocolType(
        TransportProtocolFromDescription(local_description()));
  }
  ch->SetIceCredentials(local_description_->ice_ufrag,
                        local_description_->ice_pwd);
  return true;
}

bool Transport::ApplyRemoteTransportDescription_w(TransportChannelImpl* ch,
                                                  std::string* error_desc) {
  ch->SetRemoteIceCredentials(remote_description_->ice_ufrag,
                              remote_description_->ice_pwd);
  return true;
}

bool Transport::ApplyNegotiatedTransportDescription_w(
    TransportChannelImpl* channel, std::string* error_desc) {
  ASSERT(worker_thread()->IsCurrent());
  channel->SetIceProtocolType(protocol_);
  channel->SetRemoteIceMode(remote_ice_mode_);
  return true;
}

bool Transport::NegotiateTransportDescription_w(ContentAction local_role,
                                                std::string* error_desc) {
  ASSERT(worker_thread()->IsCurrent());
  // TODO(ekr@rtfm.com): This is ICE-specific stuff. Refactor into
  // P2PTransport.
  const TransportDescription* offer;
  const TransportDescription* answer;

  if (local_role == CA_OFFER) {
    offer = local_description_.get();
    answer = remote_description_.get();
  } else {
    offer = remote_description_.get();
    answer = local_description_.get();
  }

  TransportProtocol offer_proto = TransportProtocolFromDescription(offer);
  TransportProtocol answer_proto = TransportProtocolFromDescription(answer);

  // If offered protocol is gice/ice, then we expect to receive matching
  // protocol in answer, anything else is treated as an error.
  // HYBRID is not an option when offered specific protocol.
  // If offered protocol is HYBRID and answered protocol is HYBRID then
  // gice is preferred protocol.
  // TODO(mallinath) - Answer from local or remote should't have both ice
  // and gice support. It should always pick which protocol it wants to use.
  // Once WebRTC stops supporting gice (for backward compatibility), HYBRID in
  // answer must be treated as error.
  if ((offer_proto == ICEPROTO_GOOGLE || offer_proto == ICEPROTO_RFC5245) &&
      (offer_proto != answer_proto)) {
    std::ostringstream desc;
    desc << "Offer and answer protocol mismatch: "
         << IceProtoToString(offer_proto)
         << " vs "
         << IceProtoToString(answer_proto);
    return BadTransportDescription(desc.str(), error_desc);
  }
  protocol_ = answer_proto == ICEPROTO_HYBRID ? ICEPROTO_GOOGLE : answer_proto;

  // If transport is in ICEROLE_CONTROLLED and remote end point supports only
  // ice_lite, this local end point should take CONTROLLING role.
  if (ice_role_ == ICEROLE_CONTROLLED &&
      remote_description_->ice_mode == ICEMODE_LITE) {
    SetIceRole_w(ICEROLE_CONTROLLING);
  }

  // Update remote ice_mode to all existing channels.
  remote_ice_mode_ = remote_description_->ice_mode;

  // Now that we have negotiated everything, push it downward.
  // Note that we cache the result so that if we have race conditions
  // between future SetRemote/SetLocal invocations and new channel
  // creation, we have the negotiation state saved until a new
  // negotiation happens.
  for (auto& iter : channels_) {
    if (!ApplyNegotiatedTransportDescription_w(iter.second.get(), error_desc))
      return false;
  }
  return true;
}

void Transport::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case MSG_ONSIGNALINGREADY:
      CallChannels_w(&TransportChannelImpl::OnSignalingReady);
      break;
    case MSG_ONREMOTECANDIDATE: {
        ChannelParams* params = static_cast<ChannelParams*>(msg->pdata);
        OnRemoteCandidate_w(*params->candidate);
        delete params;
      }
      break;
    case MSG_CONNECTING:
      OnConnecting_s();
      break;
    case MSG_READSTATE:
      OnChannelReadableState_s();
      break;
    case MSG_WRITESTATE:
      OnChannelWritableState_s();
      break;
    case MSG_REQUESTSIGNALING:
      OnChannelRequestSignaling_s();
      break;
    case MSG_CANDIDATEREADY:
      OnChannelCandidateReady_s();
      break;
    case MSG_ROUTECHANGE: {
        ChannelParams* params = static_cast<ChannelParams*>(msg->pdata);
        OnChannelRouteChange_s(params->channel, *params->candidate);
        delete params;
      }
      break;
    case MSG_CANDIDATEALLOCATIONCOMPLETE:
      OnChannelCandidatesAllocationDone_s();
      break;
    case MSG_ROLECONFLICT:
      SignalRoleConflict();
      break;
    case MSG_COMPLETED:
      SignalCompleted(this);
      break;
    case MSG_FAILED:
      SignalFailed(this);
      break;
  }
}

// We're GICE if the namespace is NS_GOOGLE_P2P, or if NS_JINGLE_ICE_UDP is
// used and the GICE ice-option is set.
TransportProtocol TransportProtocolFromDescription(
    const TransportDescription* desc) {
  ASSERT(desc != NULL);
  if (desc->transport_type == NS_JINGLE_ICE_UDP) {
    return (desc->HasOption(ICE_OPTION_GICE)) ?
        ICEPROTO_HYBRID : ICEPROTO_RFC5245;
  }
  return ICEPROTO_GOOGLE;
}

}  // namespace cricket

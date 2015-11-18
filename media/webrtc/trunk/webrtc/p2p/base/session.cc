/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/session.h"

#include "webrtc/p2p/base/dtlstransport.h"
#include "webrtc/p2p/base/p2ptransport.h"
#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/transportchannelproxy.h"
#include "webrtc/p2p/base/transportinfo.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/common.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/sslstreamadapter.h"

#include "webrtc/p2p/base/constants.h"

namespace cricket {

using rtc::Bind;

TransportProxy::~TransportProxy() {
  for (ChannelMap::iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {
    iter->second->SignalDestroyed(iter->second);
    delete iter->second;
  }
}

const std::string& TransportProxy::type() const {
  return transport_->get()->type();
}

TransportChannel* TransportProxy::GetChannel(int component) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  return GetChannelProxy(component);
}

TransportChannel* TransportProxy::CreateChannel(int component) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(GetChannel(component) == NULL);
  ASSERT(!transport_->get()->HasChannel(component));

  // We always create a proxy in case we need to change out the transport later.
  TransportChannelProxy* channel_proxy =
      new TransportChannelProxy(content_name(), component);
  channels_[component] = channel_proxy;

  // If we're already negotiated, create an impl and hook it up to the proxy
  // channel. If we're connecting, create an impl but don't hook it up yet.
  if (negotiated_) {
    CreateChannelImpl_w(component);
    SetChannelImplFromTransport_w(channel_proxy, component);
  } else if (connecting_) {
    CreateChannelImpl_w(component);
  }
  return channel_proxy;
}

bool TransportProxy::HasChannel(int component) {
  return transport_->get()->HasChannel(component);
}

void TransportProxy::DestroyChannel(int component) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  TransportChannelProxy* channel_proxy = GetChannelProxy(component);
  if (channel_proxy) {
    // If the state of TransportProxy is not NEGOTIATED then
    // TransportChannelProxy and its impl are not connected. Both must
    // be connected before deletion.
    //
    // However, if we haven't entered the connecting state then there
    // is no implementation to hook up.
    if (connecting_ && !negotiated_) {
      SetChannelImplFromTransport_w(channel_proxy, component);
    }

    channels_.erase(component);
    channel_proxy->SignalDestroyed(channel_proxy);
    delete channel_proxy;
  }
}

void TransportProxy::ConnectChannels() {
  if (!connecting_) {
    if (!negotiated_) {
      for (auto& iter : channels_) {
        CreateChannelImpl(iter.first);
      }
    }
    connecting_ = true;
  }
  // TODO(juberti): Right now Transport::ConnectChannels doesn't work if we
  // don't have any channels yet, so we need to allow this method to be called
  // multiple times. Once we fix Transport, we can move this call inside the
  // if (!connecting_) block.
  transport_->get()->ConnectChannels();
}

void TransportProxy::CompleteNegotiation() {
  if (!negotiated_) {
    // Negotiating assumes connecting_ has happened and
    // implementations exist. If not we need to create the
    // implementations.
    for (auto& iter : channels_) {
      if (!connecting_) {
        CreateChannelImpl(iter.first);
      }
      SetChannelImplFromTransport(iter.second, iter.first);
    }
    negotiated_ = true;
  }
}

void TransportProxy::AddSentCandidates(const Candidates& candidates) {
  for (Candidates::const_iterator cand = candidates.begin();
       cand != candidates.end(); ++cand) {
    sent_candidates_.push_back(*cand);
  }
}

void TransportProxy::AddUnsentCandidates(const Candidates& candidates) {
  for (Candidates::const_iterator cand = candidates.begin();
       cand != candidates.end(); ++cand) {
    unsent_candidates_.push_back(*cand);
  }
}

TransportChannelProxy* TransportProxy::GetChannelProxy(int component) const {
  ChannelMap::const_iterator iter = channels_.find(component);
  return (iter != channels_.end()) ? iter->second : NULL;
}

void TransportProxy::CreateChannelImpl(int component) {
  worker_thread_->Invoke<void>(Bind(
      &TransportProxy::CreateChannelImpl_w, this, component));
}

void TransportProxy::CreateChannelImpl_w(int component) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  transport_->get()->CreateChannel(component);
}

void TransportProxy::SetChannelImplFromTransport(TransportChannelProxy* proxy,
                                                 int component) {
  worker_thread_->Invoke<void>(Bind(
      &TransportProxy::SetChannelImplFromTransport_w, this, proxy, component));
}

void TransportProxy::SetChannelImplFromTransport_w(TransportChannelProxy* proxy,
                                                   int component) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  TransportChannelImpl* impl = transport_->get()->GetChannel(component);
  ASSERT(impl != NULL);
  ReplaceChannelImpl_w(proxy, impl);
}

void TransportProxy::ReplaceChannelImpl(TransportChannelProxy* proxy,
                                        TransportChannelImpl* impl) {
  worker_thread_->Invoke<void>(Bind(
      &TransportProxy::ReplaceChannelImpl_w, this, proxy, impl));
}

void TransportProxy::ReplaceChannelImpl_w(TransportChannelProxy* proxy,
                                          TransportChannelImpl* impl) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(proxy != NULL);
  proxy->SetImplementation(impl);
}

// This function muxes |this| onto |target| by repointing |this| at
// |target|'s transport and setting our TransportChannelProxies
// to point to |target|'s underlying implementations.
bool TransportProxy::SetupMux(TransportProxy* target) {
  // Bail out if there's nothing to do.
  if (transport_ == target->transport_) {
    return true;
  }

  // Run through all channels and remove any non-rtp transport channels before
  // setting target transport channels.
  for (ChannelMap::const_iterator iter = channels_.begin();
       iter != channels_.end(); ++iter) {
    if (!target->transport_->get()->HasChannel(iter->first)) {
      // Remove if channel doesn't exist in |transport_|.
      ReplaceChannelImpl(iter->second, NULL);
    } else {
      // Replace the impl for all the TransportProxyChannels with the channels
      // from |target|'s transport. Fail if there's not an exact match.
      ReplaceChannelImpl(
          iter->second, target->transport_->get()->CreateChannel(iter->first));
    }
  }

  // Now replace our transport. Must happen afterwards because
  // it deletes all impls as a side effect.
  transport_ = target->transport_;
  transport_->get()->SignalCandidatesReady.connect(
      this, &TransportProxy::OnTransportCandidatesReady);
  set_candidates_allocated(target->candidates_allocated());
  return true;
}

void TransportProxy::SetIceRole(IceRole role) {
  transport_->get()->SetIceRole(role);
}

bool TransportProxy::SetLocalTransportDescription(
    const TransportDescription& description,
    ContentAction action,
    std::string* error_desc) {
  // If this is an answer, finalize the negotiation.
  if (action == CA_ANSWER) {
    CompleteNegotiation();
  }
  bool result = transport_->get()->SetLocalTransportDescription(description,
                                                                action,
                                                                error_desc);
  if (result)
    local_description_set_ = true;
  return result;
}

bool TransportProxy::SetRemoteTransportDescription(
    const TransportDescription& description,
    ContentAction action,
    std::string* error_desc) {
  // If this is an answer, finalize the negotiation.
  if (action == CA_ANSWER) {
    CompleteNegotiation();
  }
  bool result = transport_->get()->SetRemoteTransportDescription(description,
                                                                 action,
                                                                 error_desc);
  if (result)
    remote_description_set_ = true;
  return result;
}

void TransportProxy::OnSignalingReady() {
  // If we're starting a new allocation sequence, reset our state.
  set_candidates_allocated(false);
  transport_->get()->OnSignalingReady();
}

bool TransportProxy::OnRemoteCandidates(const Candidates& candidates,
                                        std::string* error) {
  // Ensure the transport is negotiated before handling candidates.
  // TODO(juberti): Remove this once everybody calls SetLocalTD.
  CompleteNegotiation();

  // Verify each candidate before passing down to transport layer.
  for (Candidates::const_iterator cand = candidates.begin();
       cand != candidates.end(); ++cand) {
    if (!transport_->get()->VerifyCandidate(*cand, error))
      return false;
    if (!HasChannel(cand->component())) {
      *error = "Candidate has unknown component: " + cand->ToString() +
               " for content: " + content_name_;
      return false;
    }
  }
  transport_->get()->OnRemoteCandidates(candidates);
  return true;
}

void TransportProxy::SetIdentity(
    rtc::SSLIdentity* identity) {
  transport_->get()->SetIdentity(identity);
}

std::string BaseSession::StateToString(State state) {
  switch (state) {
    case STATE_INIT:
      return "STATE_INIT";
    case STATE_SENTINITIATE:
      return "STATE_SENTINITIATE";
    case STATE_RECEIVEDINITIATE:
      return "STATE_RECEIVEDINITIATE";
    case STATE_SENTPRACCEPT:
      return "STATE_SENTPRACCEPT";
    case STATE_SENTACCEPT:
      return "STATE_SENTACCEPT";
    case STATE_RECEIVEDPRACCEPT:
      return "STATE_RECEIVEDPRACCEPT";
    case STATE_RECEIVEDACCEPT:
      return "STATE_RECEIVEDACCEPT";
    case STATE_SENTMODIFY:
      return "STATE_SENTMODIFY";
    case STATE_RECEIVEDMODIFY:
      return "STATE_RECEIVEDMODIFY";
    case STATE_SENTREJECT:
      return "STATE_SENTREJECT";
    case STATE_RECEIVEDREJECT:
      return "STATE_RECEIVEDREJECT";
    case STATE_SENTREDIRECT:
      return "STATE_SENTREDIRECT";
    case STATE_SENTTERMINATE:
      return "STATE_SENTTERMINATE";
    case STATE_RECEIVEDTERMINATE:
      return "STATE_RECEIVEDTERMINATE";
    case STATE_INPROGRESS:
      return "STATE_INPROGRESS";
    case STATE_DEINIT:
      return "STATE_DEINIT";
    default:
      break;
  }
  return "STATE_" + rtc::ToString(state);
}

BaseSession::BaseSession(rtc::Thread* signaling_thread,
                         rtc::Thread* worker_thread,
                         PortAllocator* port_allocator,
                         const std::string& sid,
                         const std::string& content_type,
                         bool initiator)
    : state_(STATE_INIT),
      error_(ERROR_NONE),
      signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      port_allocator_(port_allocator),
      sid_(sid),
      content_type_(content_type),
      transport_type_(NS_GINGLE_P2P),
      initiator_(initiator),
      identity_(NULL),
      ice_tiebreaker_(rtc::CreateRandomId64()),
      role_switch_(false) {
  ASSERT(signaling_thread->IsCurrent());
}

BaseSession::~BaseSession() {
  ASSERT(signaling_thread()->IsCurrent());

  ASSERT(state_ != STATE_DEINIT);
  LogState(state_, STATE_DEINIT);
  state_ = STATE_DEINIT;
  SignalState(this, state_);

  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    delete iter->second;
  }
}

const SessionDescription* BaseSession::local_description() const {
  // TODO(tommi): Assert on thread correctness.
  return local_description_.get();
}

const SessionDescription* BaseSession::remote_description() const {
  // TODO(tommi): Assert on thread correctness.
  return remote_description_.get();
}

SessionDescription* BaseSession::remote_description() {
  // TODO(tommi): Assert on thread correctness.
  return remote_description_.get();
}

void BaseSession::set_local_description(const SessionDescription* sdesc) {
  // TODO(tommi): Assert on thread correctness.
  if (sdesc != local_description_.get())
    local_description_.reset(sdesc);
}

void BaseSession::set_remote_description(SessionDescription* sdesc) {
  // TODO(tommi): Assert on thread correctness.
  if (sdesc != remote_description_)
    remote_description_.reset(sdesc);
}

const SessionDescription* BaseSession::initiator_description() const {
  // TODO(tommi): Assert on thread correctness.
  return initiator_ ? local_description_.get() : remote_description_.get();
}

bool BaseSession::SetIdentity(rtc::SSLIdentity* identity) {
  if (identity_)
    return false;
  identity_ = identity;
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    iter->second->SetIdentity(identity_);
  }
  return true;
}

bool BaseSession::PushdownTransportDescription(ContentSource source,
                                               ContentAction action,
                                               std::string* error_desc) {
  if (source == CS_LOCAL) {
    return PushdownLocalTransportDescription(local_description(),
                                             action,
                                             error_desc);
  }
  return PushdownRemoteTransportDescription(remote_description(),
                                            action,
                                            error_desc);
}

bool BaseSession::PushdownLocalTransportDescription(
    const SessionDescription* sdesc,
    ContentAction action,
    std::string* error_desc) {
  // Update the Transports with the right information, and trigger them to
  // start connecting.
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    // If no transport info was in this session description, ret == false
    // and we just skip this one.
    TransportDescription tdesc;
    bool ret = GetTransportDescription(
        sdesc, iter->second->content_name(), &tdesc);
    if (ret) {
      if (!iter->second->SetLocalTransportDescription(tdesc, action,
                                                      error_desc)) {
        return false;
      }

      iter->second->ConnectChannels();
    }
  }

  return true;
}

bool BaseSession::PushdownRemoteTransportDescription(
    const SessionDescription* sdesc,
    ContentAction action,
    std::string* error_desc) {
  // Update the Transports with the right information.
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    TransportDescription tdesc;

    // If no transport info was in this session description, ret == false
    // and we just skip this one.
    bool ret = GetTransportDescription(
        sdesc, iter->second->content_name(), &tdesc);
    if (ret) {
      if (!iter->second->SetRemoteTransportDescription(tdesc, action,
                                                       error_desc)) {
        return false;
      }
    }
  }

  return true;
}

TransportChannel* BaseSession::CreateChannel(const std::string& content_name,
                                             int component) {
  // We create the proxy "on demand" here because we need to support
  // creating channels at any time, even before we send or receive
  // initiate messages, which is before we create the transports.
  TransportProxy* transproxy = GetOrCreateTransportProxy(content_name);
  return transproxy->CreateChannel(component);
}

TransportChannel* BaseSession::GetChannel(const std::string& content_name,
                                          int component) {
  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (transproxy == NULL)
    return NULL;

  return transproxy->GetChannel(component);
}

void BaseSession::DestroyChannel(const std::string& content_name,
                                 int component) {
  TransportProxy* transproxy = GetTransportProxy(content_name);
  ASSERT(transproxy != NULL);
  transproxy->DestroyChannel(component);
}

TransportProxy* BaseSession::GetOrCreateTransportProxy(
    const std::string& content_name) {
  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (transproxy)
    return transproxy;

  Transport* transport = CreateTransport(content_name);
  transport->SetIceRole(initiator_ ? ICEROLE_CONTROLLING : ICEROLE_CONTROLLED);
  transport->SetIceTiebreaker(ice_tiebreaker_);
  // TODO: Connect all the Transport signals to TransportProxy
  // then to the BaseSession.
  transport->SignalConnecting.connect(
      this, &BaseSession::OnTransportConnecting);
  transport->SignalWritableState.connect(
      this, &BaseSession::OnTransportWritable);
  transport->SignalRequestSignaling.connect(
      this, &BaseSession::OnTransportRequestSignaling);
  transport->SignalRouteChange.connect(
      this, &BaseSession::OnTransportRouteChange);
  transport->SignalCandidatesAllocationDone.connect(
      this, &BaseSession::OnTransportCandidatesAllocationDone);
  transport->SignalRoleConflict.connect(
      this, &BaseSession::OnRoleConflict);
  transport->SignalCompleted.connect(
      this, &BaseSession::OnTransportCompleted);
  transport->SignalFailed.connect(
      this, &BaseSession::OnTransportFailed);

  transproxy = new TransportProxy(worker_thread_, sid_, content_name,
                                  new TransportWrapper(transport));
  transproxy->SignalCandidatesReady.connect(
      this, &BaseSession::OnTransportProxyCandidatesReady);
  if (identity_)
    transproxy->SetIdentity(identity_);
  transports_[content_name] = transproxy;

  return transproxy;
}

Transport* BaseSession::GetTransport(const std::string& content_name) {
  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (transproxy == NULL)
    return NULL;
  return transproxy->impl();
}

TransportProxy* BaseSession::GetTransportProxy(
    const std::string& content_name) {
  TransportMap::iterator iter = transports_.find(content_name);
  return (iter != transports_.end()) ? iter->second : NULL;
}

void BaseSession::DestroyTransportProxy(
    const std::string& content_name) {
  TransportMap::iterator iter = transports_.find(content_name);
  if (iter != transports_.end()) {
    delete iter->second;
    transports_.erase(content_name);
  }
}

cricket::Transport* BaseSession::CreateTransport(
    const std::string& content_name) {
  ASSERT(transport_type_ == NS_GINGLE_P2P);
  return new cricket::DtlsTransport<P2PTransport>(
      signaling_thread(), worker_thread(), content_name,
      port_allocator(), identity_);
}

void BaseSession::SetState(State state) {
  ASSERT(signaling_thread_->IsCurrent());
  if (state != state_) {
    LogState(state_, state);
    state_ = state;
    SignalState(this, state_);
    signaling_thread_->Post(this, MSG_STATE);
  }
  SignalNewDescription();
}

void BaseSession::SetError(Error error, const std::string& error_desc) {
  ASSERT(signaling_thread_->IsCurrent());
  if (error != error_) {
    error_ = error;
    error_desc_ = error_desc;
    SignalError(this, error);
  }
}

void BaseSession::OnSignalingReady() {
  ASSERT(signaling_thread()->IsCurrent());
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    iter->second->OnSignalingReady();
  }
}

// TODO(juberti): Since PushdownLocalTD now triggers the connection process to
// start, remove this method once everyone calls PushdownLocalTD.
void BaseSession::SpeculativelyConnectAllTransportChannels() {
  // Put all transports into the connecting state.
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    iter->second->ConnectChannels();
  }
}

bool BaseSession::OnRemoteCandidates(const std::string& content_name,
                                     const Candidates& candidates,
                                     std::string* error) {
  // Give candidates to the appropriate transport, and tell that transport
  // to start connecting, if it's not already doing so.
  TransportProxy* transproxy = GetTransportProxy(content_name);
  if (!transproxy) {
    *error = "Unknown content name " + content_name;
    return false;
  }
  if (!transproxy->OnRemoteCandidates(candidates, error)) {
    return false;
  }
  // TODO(juberti): Remove this call once we can be sure that we always have
  // a local transport description (which will trigger the connection).
  transproxy->ConnectChannels();
  return true;
}

bool BaseSession::MaybeEnableMuxingSupport() {
  // We need both a local and remote description to decide if we should mux.
  if ((state_ == STATE_SENTINITIATE ||
      state_ == STATE_RECEIVEDINITIATE) &&
      ((local_description_ == NULL) ||
      (remote_description_ == NULL))) {
    return false;
  }

  // In order to perform the multiplexing, we need all proxies to be in the
  // negotiated state, i.e. to have implementations underneath.
  // Ensure that this is the case, regardless of whether we are going to mux.
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    ASSERT(iter->second->negotiated());
    if (!iter->second->negotiated()) {
      return false;
    }
  }

  // If both sides agree to BUNDLE, mux all the specified contents onto the
  // transport belonging to the first content name in the BUNDLE group.
  // If the contents are already muxed, this will be a no-op.
  // TODO(juberti): Should this check that local and remote have configured
  // BUNDLE the same way?
  bool candidates_allocated = IsCandidateAllocationDone();
  const ContentGroup* local_bundle_group =
      local_description_->GetGroupByName(GROUP_TYPE_BUNDLE);
  const ContentGroup* remote_bundle_group =
      remote_description_->GetGroupByName(GROUP_TYPE_BUNDLE);
  if (local_bundle_group && remote_bundle_group) {
    if (!BundleContentGroup(local_bundle_group)) {
      LOG(LS_WARNING) << "Failed to set up BUNDLE";
      return false;
    }

    // If we weren't done gathering before, we might be done now, as a result
    // of enabling mux.
    if (!candidates_allocated) {
      MaybeCandidateAllocationDone();
    }
  } else {
    LOG(LS_INFO) << "BUNDLE group missing from remote or local description.";
  }
  return true;
}

bool BaseSession::BundleContentGroup(const ContentGroup* bundle_group) {
  const std::string* content_name = bundle_group->FirstContentName();
  if (!content_name) {
    LOG(LS_INFO) << "No content names specified in BUNDLE group.";
    return true;
  }

  const ContentInfo* content =
      local_description_->GetContentByName(*content_name);
  if (!content) {
    LOG(LS_WARNING) << "Content \"" << *content_name
                    << "\" referenced in BUNDLE group"
                    << " not present in local description";
    return false;
  }

  TransportProxy* selected_proxy = GetTransportProxy(*content_name);
  if (!selected_proxy) {
    LOG(LS_WARNING) << "No transport found for content \""
                    << *content_name << "\".";
    return false;
  }

  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    // If content is part of the mux group, then repoint its proxy at the
    // transport object that we have chosen to mux onto. If the proxy
    // is already pointing at the right object, it will be a no-op.
    if (bundle_group->HasContentName(iter->first) &&
        !iter->second->SetupMux(selected_proxy)) {
      LOG(LS_WARNING) << "Failed to bundle " << iter->first << " to "
                      << *content_name;
      return false;
    }
    LOG(LS_INFO) << "Bundling " << iter->first << " to " << *content_name;
  }

  return true;
}

void BaseSession::OnTransportCandidatesAllocationDone(Transport* transport) {
  // TODO(juberti): This is a clunky way of processing the done signal. Instead,
  // TransportProxy should receive the done signal directly, set its allocated
  // flag internally, and then reissue the done signal to Session.
  // Overall we should make TransportProxy receive *all* the signals from
  // Transport, since this removes the need to manually iterate over all
  // the transports, as is needed to make sure signals are handled properly
  // when BUNDLEing.
  // TODO(juberti): Per b/7998978, devs and QA are hitting this assert in ways
  // that make it prohibitively difficult to run dbg builds. Disabled for now.
  //ASSERT(!IsCandidateAllocationDone());
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    if (iter->second->impl() == transport) {
      iter->second->set_candidates_allocated(true);
    }
  }
  MaybeCandidateAllocationDone();
}

bool BaseSession::IsCandidateAllocationDone() const {
  for (TransportMap::const_iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    if (!iter->second->candidates_allocated()) {
      LOG(LS_INFO) << "Candidate allocation not done for "
                   << iter->second->content_name();
      return false;
    }
  }
  return true;
}

void BaseSession::MaybeCandidateAllocationDone() {
  if (IsCandidateAllocationDone()) {
    LOG(LS_INFO) << "Candidate gathering is complete.";
    OnCandidatesAllocationDone();
  }
}

void BaseSession::OnRoleConflict() {
  if (role_switch_) {
    LOG(LS_WARNING) << "Repeat of role conflict signal from Transport.";
    return;
  }

  role_switch_ = true;
  for (TransportMap::iterator iter = transports_.begin();
       iter != transports_.end(); ++iter) {
    // Role will be reverse of initial role setting.
    IceRole role = initiator_ ? ICEROLE_CONTROLLED : ICEROLE_CONTROLLING;
    iter->second->SetIceRole(role);
  }
}

void BaseSession::LogState(State old_state, State new_state) {
  LOG(LS_INFO) << "Session:" << id()
               << " Old state:" << StateToString(old_state)
               << " New state:" << StateToString(new_state)
               << " Type:" << content_type()
               << " Transport:" << transport_type();
}

// static
bool BaseSession::GetTransportDescription(const SessionDescription* description,
                                          const std::string& content_name,
                                          TransportDescription* tdesc) {
  if (!description || !tdesc) {
    return false;
  }
  const TransportInfo* transport_info =
      description->GetTransportInfoByName(content_name);
  if (!transport_info) {
    return false;
  }
  *tdesc = transport_info->description;
  return true;
}

void BaseSession::SignalNewDescription() {
  ContentAction action;
  ContentSource source;
  if (!GetContentAction(&action, &source)) {
    return;
  }
  if (source == CS_LOCAL) {
    SignalNewLocalDescription(this, action);
  } else {
    SignalNewRemoteDescription(this, action);
  }
}

bool BaseSession::GetContentAction(ContentAction* action,
                                   ContentSource* source) {
  switch (state_) {
    // new local description
    case STATE_SENTINITIATE:
      *action = CA_OFFER;
      *source = CS_LOCAL;
      break;
    case STATE_SENTPRACCEPT:
      *action = CA_PRANSWER;
      *source = CS_LOCAL;
      break;
    case STATE_SENTACCEPT:
      *action = CA_ANSWER;
      *source = CS_LOCAL;
      break;
    // new remote description
    case STATE_RECEIVEDINITIATE:
      *action = CA_OFFER;
      *source = CS_REMOTE;
      break;
    case STATE_RECEIVEDPRACCEPT:
      *action = CA_PRANSWER;
      *source = CS_REMOTE;
      break;
    case STATE_RECEIVEDACCEPT:
      *action = CA_ANSWER;
      *source = CS_REMOTE;
      break;
    default:
      return false;
  }
  return true;
}

void BaseSession::OnMessage(rtc::Message *pmsg) {
  switch (pmsg->message_id) {
  case MSG_TIMEOUT:
    // Session timeout has occured.
    SetError(ERROR_TIME, "Session timeout has occured.");
    break;

  case MSG_STATE:
    switch (state_) {
    case STATE_SENTACCEPT:
    case STATE_RECEIVEDACCEPT:
      SetState(STATE_INPROGRESS);
      break;

    default:
      // Explicitly ignoring some states here.
      break;
    }
    break;
  }
}

}  // namespace cricket

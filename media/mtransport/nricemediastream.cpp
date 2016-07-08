/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

// Some of this code is cut-and-pasted from nICEr. Copyright is:

/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <string>
#include <vector>

#include "logging.h"
#include "nsError.h"

// nICEr includes
extern "C" {
#include "nr_api.h"
#include "registry.h"
#include "async_timer.h"
#include "ice_util.h"
#include "transport_addr.h"
#include "nr_crypto.h"
#include "nr_socket.h"
#include "nr_socket_local.h"
#include "stun_client_ctx.h"
#include "stun_server_ctx.h"
#include "ice_ctx.h"
#include "ice_candidate.h"
#include "ice_handler.h"
}

// Local includes
#include "nricectx.h"
#include "nricemediastream.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

static bool ToNrIceAddr(nr_transport_addr &addr,
                        NrIceAddr *out) {
  int r;
  char addrstring[INET6_ADDRSTRLEN + 1];

  r = nr_transport_addr_get_addrstring(&addr, addrstring, sizeof(addrstring));
  if (r)
    return false;
  out->host = addrstring;

  int port;
  r = nr_transport_addr_get_port(&addr, &port);
  if (r)
    return false;

  out->port = port;

  switch (addr.protocol) {
    case IPPROTO_TCP:
      out->transport = kNrIceTransportTcp;
      break;
    case IPPROTO_UDP:
      out->transport = kNrIceTransportUdp;
      break;
    default:
      MOZ_CRASH();
      return false;
  }

  return true;
}

static bool ToNrIceCandidate(const nr_ice_candidate& candc,
                             NrIceCandidate* out) {
  MOZ_ASSERT(out);
  int r;
  // Const-cast because the internal nICEr code isn't const-correct.
  nr_ice_candidate *cand = const_cast<nr_ice_candidate *>(&candc);

  if (!ToNrIceAddr(cand->addr, &out->cand_addr))
    return false;

  if (cand->isock) {
    nr_transport_addr addr;
    r = nr_socket_getaddr(cand->isock->sock, &addr);
    if (r)
      return false;

    if (!ToNrIceAddr(addr, &out->local_addr))
      return false;
  }

  NrIceCandidate::Type type;

  switch (cand->type) {
    case HOST:
      type = NrIceCandidate::ICE_HOST;
      break;
    case SERVER_REFLEXIVE:
      type = NrIceCandidate::ICE_SERVER_REFLEXIVE;
      break;
    case PEER_REFLEXIVE:
      type = NrIceCandidate::ICE_PEER_REFLEXIVE;
      break;
    case RELAYED:
      type = NrIceCandidate::ICE_RELAYED;
      break;
    default:
      return false;
  }

  NrIceCandidate::TcpType tcp_type;
  switch (cand->tcp_type) {
    case TCP_TYPE_ACTIVE:
      tcp_type = NrIceCandidate::ICE_ACTIVE;
      break;
    case TCP_TYPE_PASSIVE:
      tcp_type = NrIceCandidate::ICE_PASSIVE;
      break;
    case TCP_TYPE_SO:
      tcp_type = NrIceCandidate::ICE_SO;
      break;
    default:
      tcp_type = NrIceCandidate::ICE_NONE;
      break;
  }

  out->type = type;
  out->tcp_type = tcp_type;
  out->codeword = candc.codeword;
  return true;
}

// Make an NrIceCandidate from the candidate |cand|.
// This is not a member fxn because we want to hide the
// defn of nr_ice_candidate but we pass by reference.
static UniquePtr<NrIceCandidate> MakeNrIceCandidate(const nr_ice_candidate& candc) {
  UniquePtr<NrIceCandidate> out(new NrIceCandidate());

  if (!ToNrIceCandidate(candc, out.get())) {
    return nullptr;
  }
  return out;
}

// NrIceMediaStream
RefPtr<NrIceMediaStream>
NrIceMediaStream::Create(NrIceCtx *ctx,
                         const std::string& name,
                         int components) {
  RefPtr<NrIceMediaStream> stream =
    new NrIceMediaStream(ctx, name, components);
  MOZ_ASSERT(stream->ctx_ == ctx->ctx());

  int r = nr_ice_add_media_stream(ctx->ctx(),
                                  const_cast<char *>(name.c_str()),
                                  components, &stream->stream_);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't create ICE media stream for '"
              << name << "'");
    return nullptr;
  }

  return stream;
}

NrIceMediaStream::NrIceMediaStream(NrIceCtx *ctx,
                                   const std::string& name,
                                   size_t components) :
      state_(ICE_CONNECTING),
      ctx_(ctx->ctx()),
      ctx_peer_(ctx->peer()),
      name_(name),
      components_(components),
      stream_(nullptr),
      level_(0),
      has_parsed_attrs_(false)
{
}

NrIceMediaStream::~NrIceMediaStream() {
  // We do not need to destroy anything. All major resources
  // are attached to the ice ctx.
}

nsresult NrIceMediaStream::ParseAttributes(std::vector<std::string>&
                                           attributes) {
  if (!stream_)
    return NS_ERROR_FAILURE;

  std::vector<char *> attributes_in;

  for (size_t i=0; i<attributes.size(); ++i) {
    attributes_in.push_back(const_cast<char *>(attributes[i].c_str()));
  }

  // Still need to call nr_ice_ctx_parse_stream_attributes.
  int r = nr_ice_peer_ctx_parse_stream_attributes(ctx_peer_,
                                                  stream_,
                                                  attributes_in.size() ?
                                                  &attributes_in[0] : nullptr,
                                                  attributes_in.size());
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't parse attributes for stream "
              << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  has_parsed_attrs_ = true;
  return NS_OK;
}

// Parse trickle ICE candidate
nsresult NrIceMediaStream::ParseTrickleCandidate(const std::string& candidate) {
  int r;

  MOZ_MTLOG(ML_DEBUG, "NrIceCtx(" << ctx_->label << ")/STREAM(" <<
            name() << ") : parsing trickle candidate " << candidate);

  r = nr_ice_peer_ctx_parse_trickle_candidate(ctx_peer_,
                                              stream_,
                                              const_cast<char *>(
                                                candidate.c_str())
                                              );
  if (r) {
    if (r == R_ALREADY) {
      MOZ_MTLOG(ML_ERROR, "Trickle candidates are redundant for stream '"
                << name_ << "' because it is completed");

    } else {
      MOZ_MTLOG(ML_ERROR, "Couldn't parse trickle candidate for stream '"
                << name_ << "'");
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

// Returns NS_ERROR_NOT_AVAILABLE if component is unpaired or disabled.
nsresult NrIceMediaStream::GetActivePair(int component,
                                         UniquePtr<NrIceCandidate>* localp,
                                         UniquePtr<NrIceCandidate>* remotep) {
  int r;
  nr_ice_candidate *local_int;
  nr_ice_candidate *remote_int;

  if (!stream_) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  r = nr_ice_media_stream_get_active(ctx_peer_,
                                     stream_,
                                     component,
                                     &local_int, &remote_int);
  // If result is R_REJECTED then component is unpaired or disabled.
  if (r == R_REJECTED)
    return NS_ERROR_NOT_AVAILABLE;

  if (r)
    return NS_ERROR_FAILURE;

  UniquePtr<NrIceCandidate> local(
      MakeNrIceCandidate(*local_int));
  if (!local)
    return NS_ERROR_FAILURE;

  UniquePtr<NrIceCandidate> remote(
      MakeNrIceCandidate(*remote_int));
  if (!remote)
    return NS_ERROR_FAILURE;

  if (localp)
    *localp = Move(local);
  if (remotep)
    *remotep = Move(remote);

  return NS_OK;
}


nsresult NrIceMediaStream::GetCandidatePairs(std::vector<NrIceCandidatePair>*
                                             out_pairs) const {
  MOZ_ASSERT(out_pairs);
  if (!stream_) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If we haven't at least started checking then there is nothing to report
  if (ctx_peer_->state != NR_ICE_PEER_STATE_PAIRED) {
    return NS_OK;
  }

  // Get the check_list on the peer stream (this is where the check_list
  // actually lives, not in stream_)
  nr_ice_media_stream* peer_stream;
  int r = nr_ice_peer_ctx_find_pstream(ctx_peer_, stream_, &peer_stream);
  if (r != 0) {
    return NS_ERROR_FAILURE;
  }

  nr_ice_cand_pair *p1, *p2;
  out_pairs->clear();

  TAILQ_FOREACH(p1, &peer_stream->check_list, check_queue_entry) {
    MOZ_ASSERT(p1);
    MOZ_ASSERT(p1->local);
    MOZ_ASSERT(p1->remote);
    NrIceCandidatePair pair;

    p2 = TAILQ_FIRST(&peer_stream->check_list);
    while (p2) {
      if (p1 == p2) {
        /* Don't compare with our self. */
        p2=TAILQ_NEXT(p2, check_queue_entry);
        continue;
      }
      if (strncmp(p1->codeword,p2->codeword,sizeof(p1->codeword))==0) {
        /* In case of duplicate pairs we only report the one winning pair */
        if (
            ((p2->remote->component && (p2->remote->component->active == p2)) &&
             !(p1->remote->component && (p1->remote->component->active == p1))) ||
            ((p2->peer_nominated || p2->nominated) &&
             !(p1->peer_nominated || p1->nominated)) ||
            (p2->priority > p1->priority) ||
            ((p2->state == NR_ICE_PAIR_STATE_SUCCEEDED) &&
             (p1->state != NR_ICE_PAIR_STATE_SUCCEEDED)) ||
            ((p2->state != NR_ICE_PAIR_STATE_CANCELLED) &&
             (p1->state == NR_ICE_PAIR_STATE_CANCELLED))
            ) {
          /* p2 is a better pair. */
          break;
        }
      }
      p2=TAILQ_NEXT(p2, check_queue_entry);
    }
    if (p2) {
      /* p2 points to a duplicate but better pair so skip this one */
      continue;
    }

    switch (p1->state) {
      case NR_ICE_PAIR_STATE_FROZEN:
        pair.state = NrIceCandidatePair::State::STATE_FROZEN;
        break;
      case NR_ICE_PAIR_STATE_WAITING:
        pair.state = NrIceCandidatePair::State::STATE_WAITING;
        break;
      case NR_ICE_PAIR_STATE_IN_PROGRESS:
        pair.state = NrIceCandidatePair::State::STATE_IN_PROGRESS;
        break;
      case NR_ICE_PAIR_STATE_FAILED:
        pair.state = NrIceCandidatePair::State::STATE_FAILED;
        break;
      case NR_ICE_PAIR_STATE_SUCCEEDED:
        pair.state = NrIceCandidatePair::State::STATE_SUCCEEDED;
        break;
      case NR_ICE_PAIR_STATE_CANCELLED:
        pair.state = NrIceCandidatePair::State::STATE_CANCELLED;
        break;
      default:
        MOZ_ASSERT(0);
    }

    pair.priority = p1->priority;
    pair.nominated = p1->peer_nominated || p1->nominated;
    pair.selected = p1->remote->component &&
                    p1->remote->component->active == p1;
    pair.codeword = p1->codeword;

    if (!ToNrIceCandidate(*(p1->local), &pair.local) ||
        !ToNrIceCandidate(*(p1->remote), &pair.remote)) {
      return NS_ERROR_FAILURE;
    }

    out_pairs->push_back(pair);
  }

  return NS_OK;
}

nsresult NrIceMediaStream::GetDefaultCandidate(
    int component,
    NrIceCandidate* candidate) const {

  nr_ice_candidate *cand;

  int r = nr_ice_media_stream_get_default_candidate(stream_, component, &cand);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't get default ICE candidate for '"
              << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  if (!ToNrIceCandidate(*cand, candidate)) {
    MOZ_MTLOG(ML_ERROR, "Failed to convert default ICE candidate for '"
              << name_ << "'");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

std::vector<std::string> NrIceMediaStream::GetCandidates() const {
  char **attrs = 0;
  int attrct;
  int r;
  std::vector<std::string> ret;

  if (!stream_) {
    return ret;
  }

  r = nr_ice_media_stream_get_attributes(stream_,
                                         &attrs, &attrct);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't get ICE candidates for '"
              << name_ << "'");
    return ret;
  }

  for (int i=0; i<attrct; i++) {
    ret.push_back(attrs[i]);
    RFREE(attrs[i]);
  }

  RFREE(attrs);

  return ret;
}

static nsresult GetCandidatesFromStream(
    nr_ice_media_stream *stream,
    std::vector<NrIceCandidate> *candidates) {
  MOZ_ASSERT(candidates);
  nr_ice_component* comp=STAILQ_FIRST(&stream->components);
  while(comp){
    if (comp->state != NR_ICE_COMPONENT_DISABLED) {
      nr_ice_candidate *cand = TAILQ_FIRST(&comp->candidates);
      while(cand){
        NrIceCandidate new_cand;
        // This can fail if the candidate is server reflexive or relayed, and
        // has not yet received a response (ie; it doesn't know its address
        // yet). For the purposes of this code, this isn't a candidate we're
        // interested in, since it is not fully baked yet.
        if (ToNrIceCandidate(*cand, &new_cand)) {
          candidates->push_back(new_cand);
        }
        cand=TAILQ_NEXT(cand,entry_comp);
      }
    }
    comp=STAILQ_NEXT(comp,entry);
  }

  return NS_OK;
}

nsresult NrIceMediaStream::GetLocalCandidates(
    std::vector<NrIceCandidate>* candidates) const {
  if (!stream_) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return GetCandidatesFromStream(stream_, candidates);
}

nsresult NrIceMediaStream::GetRemoteCandidates(
    std::vector<NrIceCandidate>* candidates) const {
  if (!stream_) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If we haven't at least started checking then there is nothing to report
  if (ctx_peer_->state != NR_ICE_PEER_STATE_PAIRED) {
    return NS_OK;
  }

  nr_ice_media_stream* peer_stream;
  int r = nr_ice_peer_ctx_find_pstream(ctx_peer_, stream_, &peer_stream);
  if (r != 0) {
    return NS_ERROR_FAILURE;
  }

  return GetCandidatesFromStream(peer_stream, candidates);
}


nsresult NrIceMediaStream::DisableComponent(int component_id) {
  if (!stream_)
    return NS_ERROR_FAILURE;

  int r = nr_ice_media_stream_disable_component(stream_,
                                                component_id);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable '" << name_ << "':" <<
              component_id);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult NrIceMediaStream::GetConsentStatus(int component_id, bool *can_send, struct timeval *ts) {
  if (!stream_)
    return NS_ERROR_FAILURE;

  nr_ice_media_stream* peer_stream;
  int r = nr_ice_peer_ctx_find_pstream(ctx_peer_, stream_, &peer_stream);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Failed to find peer stream for '" << name_ << "':" <<
              component_id);
    return NS_ERROR_FAILURE;
  }

  int send = 0;
  r = nr_ice_media_stream_get_consent_status(peer_stream, component_id,
                                             &send, ts);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Failed to get consent status for '" << name_ << "':" <<
              component_id);
    return NS_ERROR_FAILURE;
  }
  *can_send = !!send;

  return NS_OK;
}

nsresult NrIceMediaStream::SendPacket(int component_id,
                                      const unsigned char *data,
                                      size_t len) {
  if (!stream_)
    return NS_ERROR_FAILURE;

  int r = nr_ice_media_stream_send(ctx_peer_, stream_,
                                   component_id,
                                   const_cast<unsigned char *>(data), len);
  if (r) {
    MOZ_MTLOG(ML_ERROR, "Couldn't send media on '" << name_ << "'");
    if (r == R_WOULDBLOCK) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    return NS_BASE_STREAM_OSERROR;
  }

  return NS_OK;
}


void NrIceMediaStream::Ready() {
  // This function is called whenever a stream becomes ready, but it
  // gets fired multiple times when a stream gets nominated repeatedly.
  if (state_ != ICE_OPEN) {
    MOZ_MTLOG(ML_DEBUG, "Marking stream ready '" << name_ << "'");
    state_ = ICE_OPEN;
    SignalReady(this);
  }
  else {
    MOZ_MTLOG(ML_DEBUG, "Stream ready callback fired again for '" << name_ << "'");
  }
}

void NrIceMediaStream::Close() {
  MOZ_MTLOG(ML_DEBUG, "Marking stream closed '" << name_ << "'");
  state_ = ICE_CLOSED;

  if (stream_) {
    int r = nr_ice_remove_media_stream(ctx_, &stream_);
    if (r) {
      MOZ_ASSERT(false, "Failed to remove stream");
      MOZ_MTLOG(ML_ERROR, "Failed to remove stream, error=" << r);
    }
  }
}

}  // close namespace

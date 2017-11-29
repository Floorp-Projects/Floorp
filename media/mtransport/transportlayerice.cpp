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

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsNetCID.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

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
#include "logging.h"
#include "nricectx.h"
#include "nricemediastream.h"
#include "transportflow.h"
#include "transportlayerice.h"

namespace mozilla {

#ifdef ERROR
#undef ERROR
#endif

MOZ_MTLOG_MODULE("mtransport")

TransportLayerIce::TransportLayerIce()
    : stream_(nullptr), component_(0),
      old_stream_(nullptr)
{
  // setup happens later
}

TransportLayerIce::~TransportLayerIce() {
  // No need to do anything here, since we use smart pointers
}

void TransportLayerIce::SetParameters(RefPtr<NrIceMediaStream> stream,
                                      int component) {
  // Stream could be null in the case of some badly written js that causes
  // us to be in an ICE restart case, but not have valid streams due to
  // not calling PeerConnectionMedia::EnsureTransports if
  // PeerConnectionImpl::SetSignalingState_m thinks the conditions were
  // not correct.  We also solved a case where an incoming answer was
  // incorrectly beginning an ICE restart when the offer did not indicate one.
  if (!stream) {
    MOZ_ASSERT(false);
    return;
  }

  // If SetParameters is called and we already have a stream_, this means
  // we're handling an ICE restart.  We need to hold the old stream until
  // we know the new stream is working.
  if (stream_ && !old_stream_ && (stream_ != stream)) {
    // Here we leave the old stream's signals connected until we don't need
    // it anymore.  They will be disconnected if ice restart is successful.
    old_stream_ = stream_;
    MOZ_MTLOG(ML_INFO, LAYER_INFO << "SetParameters save old stream("
                                  << old_stream_->name() << ")");
  }

  stream_ = stream;
  component_ = component;

  PostSetup();
}

void TransportLayerIce::PostSetup() {
  stream_->SignalReady.connect(this, &TransportLayerIce::IceReady);
  stream_->SignalFailed.connect(this, &TransportLayerIce::IceFailed);
  stream_->SignalPacketReceived.connect(this,
                                        &TransportLayerIce::IcePacketReceived);
  if (stream_->state() == NrIceMediaStream::ICE_OPEN) {
    TL_SET_STATE(TS_OPEN);
  }
}

void TransportLayerIce::ResetOldStream() {
  if (old_stream_ == nullptr) {
    return; // no work to do
  }
  // ICE restart successful on the new stream, we can forget the old stream now
  MOZ_MTLOG(ML_INFO, LAYER_INFO << "ResetOldStream(" << old_stream_->name()
                                << ")");
  old_stream_->SignalReady.disconnect(this);
  old_stream_->SignalFailed.disconnect(this);
  old_stream_->SignalPacketReceived.disconnect(this);
  old_stream_ = nullptr;
}

void TransportLayerIce::RestoreOldStream() {
  if (old_stream_ == nullptr) {
    return; // no work to do
  }
  // ICE restart rollback, we need to restore the old stream
  MOZ_MTLOG(ML_INFO, LAYER_INFO << "RestoreOldStream(" << old_stream_->name()
                                << ")");
  stream_->SignalReady.disconnect(this);
  stream_->SignalFailed.disconnect(this);
  stream_->SignalPacketReceived.disconnect(this);
  stream_ = old_stream_;
  old_stream_ = nullptr;

  if (stream_->state() == NrIceMediaStream::ICE_OPEN) {
    IceReady(stream_);
  } else if (stream_->state() == NrIceMediaStream::ICE_CLOSED) {
    IceFailed(stream_);
  }
  // No events are fired when the stream is ICE_CONNECTING.  If the
  // restored stream is ICE_CONNECTING, IceReady/IceFailed will fire
  // later.
}

TransportResult TransportLayerIce::SendPacket(const unsigned char *data,
                                              size_t len) {
  CheckThread();
  // use old_stream_ until stream_ is ready
  nsresult res = (old_stream_?old_stream_:stream_)->SendPacket(component_,
                                                               data,
                                                               len);

  if (!NS_SUCCEEDED(res)) {
    return (res == NS_BASE_STREAM_WOULD_BLOCK) ?
        TE_WOULDBLOCK : TE_ERROR;
  }

  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << " SendPacket(" << len << ") succeeded");

  return len;
}


void TransportLayerIce::IceCandidate(NrIceMediaStream *stream,
                                     const std::string&) {
  // NO-OP for now
}

void TransportLayerIce::IceReady(NrIceMediaStream *stream) {
  CheckThread();
  // only handle the current stream (not the old stream during restart)
  if (stream != stream_) {
    return;
  }
  MOZ_MTLOG(ML_INFO, LAYER_INFO << "ICE Ready(" << stream->name() << ","
    << component_ << ")");
  TL_SET_STATE(TS_OPEN);
}

void TransportLayerIce::IceFailed(NrIceMediaStream *stream) {
  CheckThread();
  // only handle the current stream (not the old stream during restart)
  if (stream != stream_) {
    return;
  }
  MOZ_MTLOG(ML_INFO, LAYER_INFO << "ICE Failed(" << stream->name() << ","
    << component_ << ")");
  TL_SET_STATE(TS_ERROR);
}

void TransportLayerIce::IcePacketReceived(NrIceMediaStream *stream, int component,
                       const unsigned char *data, int len) {
  CheckThread();
  // We get packets for both components, so ignore the ones that aren't
  // for us.
  if (component_ != component)
    return;

  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "PacketReceived(" << stream->name() << ","
    << component << "," << len << ")");
  SignalPacketReceived(this, data, len);
}

}  // close namespace

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

// This is a wrapper around the nICEr ICE stack
#ifndef nricemediastream_h__
#define nricemediastream_h__

#include <string>
#include <vector>

#include "sigslot.h"

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"

#include "m_cpp_utils.h"


namespace mozilla {

typedef struct nr_ice_ctx_ nr_ice_ctx;
typedef struct nr_ice_peer_ctx_ nr_ice_peer_ctx;
typedef struct nr_ice_media_stream_ nr_ice_media_stream;

class NrIceCtx;

struct NrIceAddr {
  std::string host;
  uint16_t port;
  std::string transport;
};

/* A summary of a candidate, for use in asking which candidate
   pair is active */
struct NrIceCandidate {
  enum Type {
    ICE_HOST,
    ICE_SERVER_REFLEXIVE,
    ICE_PEER_REFLEXIVE,
    ICE_RELAYED
  };

  enum TcpType {
    ICE_NONE,
    ICE_ACTIVE,
    ICE_PASSIVE,
    ICE_SO
  };

  NrIceAddr cand_addr;
  NrIceAddr local_addr;
  Type type;
  TcpType tcp_type;
  std::string codeword;
};

struct NrIceCandidatePair {

  enum State {
    STATE_FROZEN,
    STATE_WAITING,
    STATE_IN_PROGRESS,
    STATE_FAILED,
    STATE_SUCCEEDED,
    STATE_CANCELLED
  };

  State state;
  uint64_t priority;
  // Set regardless of who nominated it. Does not necessarily mean that it is
  // ready to be selected (ie; nominated by peer, but our check has not
  // succeeded yet.) Note: since this implementation uses aggressive nomination,
  // when we are the controlling agent, this will always be set if the pair is
  // in STATE_SUCCEEDED.
  bool nominated;
  // Set if this candidate pair has been selected. Note: Since we are using
  // aggressive nomination, this could change frequently as ICE runs.
  bool selected;
  NrIceCandidate local;
  NrIceCandidate remote;
  // TODO(bcampen@mozilla.com): Is it important to put the foundation in here?
  std::string codeword;
};

class NrIceMediaStream {
 public:
  static RefPtr<NrIceMediaStream> Create(NrIceCtx *ctx,
                                         const std::string& name,
                                         int components);
  enum State { ICE_CONNECTING, ICE_OPEN, ICE_CLOSED};

  State state() const { return state_; }

  // The name of the stream
  const std::string& name() const { return name_; }

  // Get all the candidates
  std::vector<std::string> GetCandidates() const;

  nsresult GetLocalCandidates(std::vector<NrIceCandidate>* candidates) const;
  nsresult GetRemoteCandidates(std::vector<NrIceCandidate>* candidates) const;

  // Get all candidate pairs, whether in the check list or triggered check
  // queue, in priority order. |out_pairs| is cleared before being filled.
  nsresult GetCandidatePairs(std::vector<NrIceCandidatePair>* out_pairs) const;

  nsresult GetDefaultCandidate(int component, NrIceCandidate* candidate) const;

  // Parse remote attributes
  nsresult ParseAttributes(std::vector<std::string>& candidates);
  bool HasParsedAttributes() const { return has_parsed_attrs_; }

  // Parse trickle ICE candidate
  nsresult ParseTrickleCandidate(const std::string& candidate);

  // Disable a component
  nsresult DisableComponent(int component);

  // Get the candidate pair currently active. It's the
  // caller's responsibility to free these.
  nsresult GetActivePair(int component,
                         UniquePtr<NrIceCandidate>* local,
                         UniquePtr<NrIceCandidate>* remote);

  // Get the current ICE consent send status plus the timeval of the last
  // consent update time.
  nsresult GetConsentStatus(int component, bool *can_send, struct timeval *ts);

  // The number of components
  size_t components() const { return components_; }

  // The underlying nICEr stream
  nr_ice_media_stream *stream() { return stream_; }
  // Signals to indicate events. API users can (and should)
  // register for these.

  // Send a packet
  nsresult SendPacket(int component_id, const unsigned char *data, size_t len);

  // Set your state to ready. Called by the NrIceCtx;
  void Ready();

  // Close the stream. Called by the NrIceCtx.
  // Different from the destructor because other people
  // might be holding RefPtrs but we want those writes to fail once
  // the context has been destroyed.
  void Close();

  // So the receiver of SignalCandidate can determine which level
  // (ie; m-line index) the candidate belongs to.
  void SetLevel(uint16_t level) { level_ = level; }

  uint16_t GetLevel() const { return level_; }

  sigslot::signal2<NrIceMediaStream *, const std::string& >
  SignalCandidate;  // A new ICE candidate:

  sigslot::signal1<NrIceMediaStream *> SignalReady;  // Candidate pair ready.
  sigslot::signal1<NrIceMediaStream *> SignalFailed;  // Candidate pair failed.
  sigslot::signal4<NrIceMediaStream *, int, const unsigned char *, int>
  SignalPacketReceived;  // Incoming packet

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NrIceMediaStream)

 private:
  NrIceMediaStream(NrIceCtx *ctx,
                   const std::string& name,
                   size_t components);

  ~NrIceMediaStream();

  DISALLOW_COPY_ASSIGN(NrIceMediaStream);

  State state_;
  nr_ice_ctx *ctx_;
  nr_ice_peer_ctx *ctx_peer_;
  const std::string name_;
  const size_t components_;
  nr_ice_media_stream *stream_;
  uint16_t level_;
  bool has_parsed_attrs_;
};


}  // close namespace
#endif

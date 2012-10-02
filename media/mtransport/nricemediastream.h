/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/**
   nricemediastream.h

   Copyright (C) 2012, RTFM, Inc.
   All Rights Reserved.

   ekr@rtfm.com  Mon Jul 30 09:48:47 2012
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

// This is a wrapper around the nICEr ICE stack
#ifndef nricemediastream_h__
#define nricemediastream_h__

#include <vector>

#include "sigslot.h"

#include "mozilla/RefPtr.h"
#include "mozilla/Scoped.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"

#include "m_cpp_utils.h"


namespace mozilla {

typedef struct nr_ice_media_stream_ nr_ice_media_stream;

class NrIceCtx;

class NrIceMediaStream {
 public:
  static RefPtr<NrIceMediaStream> Create(NrIceCtx *ctx,
                                         const std::string& name,
                                         int components);
  ~NrIceMediaStream();

  enum State { ICE_CONNECTING, ICE_OPEN, ICE_CLOSED};

  State state() const { return state_; }

  // The name of the stream
  const std::string& name() const { return name_; }

  // Get all the candidates
  std::vector<std::string> GetCandidates() const;

  // Get the default candidate as host and port
  nsresult GetDefaultCandidate(int component, std::string *host, int *port);

  // Parse remote attributes
  nsresult ParseAttributes(std::vector<std::string>& candidates);

  // Parse trickle ICE candidate
  nsresult ParseTrickleCandidate(const std::string& candidate);

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

  sigslot::signal2<NrIceMediaStream *, const std::string& >
  SignalCandidate;  // A new ICE candidate:
  sigslot::signal1<NrIceMediaStream *> SignalReady;  // Candidate pair ready.
  sigslot::signal1<NrIceMediaStream *> SignalFailed;  // Candidate pair failed.
  sigslot::signal4<NrIceMediaStream *, int, const unsigned char *, int>
  SignalPacketReceived;  // Incoming packet

  // Emit all the ICE candidates. Note that this doesn't
  // work for trickle ICE yet--called internally
  void EmitAllCandidates();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NrIceMediaStream);

 private:
  NrIceMediaStream(NrIceCtx *ctx,  const std::string& name,
                   int components) :
      state_(ICE_CONNECTING),
      ctx_(ctx),
      name_(name),
      components_(components),
      stream_(nullptr)  {}

  DISALLOW_COPY_ASSIGN(NrIceMediaStream);

  State state_;
  NrIceCtx *ctx_;
  const std::string name_;
  const int components_;
  nr_ice_media_stream *stream_;
};


}  // close namespace
#endif

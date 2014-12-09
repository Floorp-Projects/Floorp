/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

// This is a wrapper around the nICEr ICE stack
#ifndef transportlayerice_h__
#define transportlayerice_h__

#include <vector>

#include "sigslot.h"

#include "mozilla/RefPtr.h"
#include "mozilla/Scoped.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"

#include "m_cpp_utils.h"

#include "nricemediastream.h"
#include "transportflow.h"
#include "transportlayer.h"

// An ICE transport layer -- corresponds to a single ICE
namespace mozilla {

class TransportLayerIce : public TransportLayer {
 public:
  TransportLayerIce(const std::string& name,
                    RefPtr<NrIceCtx> ctx,
                    RefPtr<NrIceMediaStream> stream,
                    int component);
  virtual ~TransportLayerIce();

  // Transport layer overrides.
  virtual TransportResult SendPacket(const unsigned char *data, size_t len);

  // Slots for ICE
  void IceCandidate(NrIceMediaStream *stream, const std::string&);
  void IceReady(NrIceMediaStream *stream);
  void IceFailed(NrIceMediaStream *stream);
  void IcePacketReceived(NrIceMediaStream *stream, int component,
                         const unsigned char *data, int len);

  TRANSPORT_LAYER_ID("ice")

 private:
  DISALLOW_COPY_ASSIGN(TransportLayerIce);

  const std::string name_;
  RefPtr<NrIceCtx> ctx_;
  RefPtr<NrIceMediaStream> stream_;
  int component_;
};

}  // close namespace
#endif

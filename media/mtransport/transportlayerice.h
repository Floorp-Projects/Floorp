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
  TransportLayerIce();

  virtual ~TransportLayerIce();

  void SetParameters(RefPtr<NrIceMediaStream> stream,
                     int component);

  void ResetOldStream(); // called after successful ice restart
  void RestoreOldStream(); // called after unsuccessful ice restart

  // Transport layer overrides.
  TransportResult SendPacket(MediaPacket& packet) override;

  // Slots for ICE
  void IceCandidate(NrIceMediaStream *stream, const std::string&);
  void IceReady(NrIceMediaStream *stream);
  void IceFailed(NrIceMediaStream *stream);
  void IcePacketReceived(NrIceMediaStream *stream, int component,
                         const unsigned char *data, int len);

  TRANSPORT_LAYER_ID("ice")

 private:
  DISALLOW_COPY_ASSIGN(TransportLayerIce);
  void PostSetup();

  RefPtr<NrIceMediaStream> stream_;
  int component_;

  // used to hold the old stream
  RefPtr<NrIceMediaStream> old_stream_;
};

}  // close namespace
#endif

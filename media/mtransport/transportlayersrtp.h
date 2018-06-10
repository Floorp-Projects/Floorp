/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef transportlayersrtp_h__
#define transportlayersrtp_h__

#include <string>

#include "transportlayer.h"
#include "mozilla/RefPtr.h"
#include "SrtpFlow.h"

namespace mozilla {

class TransportLayerDtls;

class TransportLayerSrtp final : public TransportLayer {
  public:
    explicit TransportLayerSrtp(TransportLayerDtls& dtls);
    virtual ~TransportLayerSrtp() {};

    // Transport layer overrides.
    void WasInserted() override;
    TransportResult SendPacket(MediaPacket& packet) override;

    // Signals
    void StateChange(TransportLayer *layer, State state);
    void PacketReceived(TransportLayer* layer, MediaPacket& packet);

    TRANSPORT_LAYER_ID("srtp")

  private:
    bool Setup();
    DISALLOW_COPY_ASSIGN(TransportLayerSrtp);
    RefPtr<SrtpFlow> mSendSrtp;
    RefPtr<SrtpFlow> mRecvSrtp;
};


}  // close namespace
#endif

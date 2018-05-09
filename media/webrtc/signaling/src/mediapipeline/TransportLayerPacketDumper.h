/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef transportlayerpacketdumper_h__
#define transportlayerpacketdumper_h__

#include "transportlayer.h"
#include "signaling/src/peerconnection/PacketDumper.h"
#include "mozilla/dom/RTCPeerConnectionBinding.h"

namespace mozilla {

class TransportLayerPacketDumper final : public TransportLayer {
  public:
    explicit TransportLayerPacketDumper(nsAutoPtr<PacketDumper>&& aPacketDumper,
                                        dom::mozPacketDumpType aType);
    virtual ~TransportLayerPacketDumper() {};

    // Transport layer overrides.
    void WasInserted() override;
    TransportResult SendPacket(MediaPacket& packet) override;

    // Signals
    void StateChange(TransportLayer *aLayer, State state);
    void PacketReceived(TransportLayer* aLayer, MediaPacket& packet);

    TRANSPORT_LAYER_ID("packet-dumper")

  private:
    DISALLOW_COPY_ASSIGN(TransportLayerPacketDumper);
    nsAutoPtr<PacketDumper> mPacketDumper;
    dom::mozPacketDumpType mType;
};


}  // close namespace
#endif

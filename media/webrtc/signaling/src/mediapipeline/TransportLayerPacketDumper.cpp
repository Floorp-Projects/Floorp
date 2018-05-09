/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "TransportLayerPacketDumper.h"

#include "logging.h"
#include "nsError.h"
#include "mozilla/Assertions.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

TransportLayerPacketDumper::TransportLayerPacketDumper(
    nsAutoPtr<PacketDumper>&& aPacketDumper, dom::mozPacketDumpType aType) :
  mPacketDumper(std::move(aPacketDumper)),
  mType(aType)
{}

void
TransportLayerPacketDumper::WasInserted()
{
  CheckThread();
  if (!downward_) {
    MOZ_MTLOG(ML_ERROR, "Packet dumper with nothing below. This is useless");
    TL_SET_STATE(TS_ERROR);
  }

  downward_->SignalStateChange.connect(this,
      &TransportLayerPacketDumper::StateChange);
  downward_->SignalPacketReceived.connect(this,
      &TransportLayerPacketDumper::PacketReceived);
}

TransportResult
TransportLayerPacketDumper::SendPacket(MediaPacket& packet)
{
  if (packet.sdp_level().isSome()) {
    dom::mozPacketDumpType dumpType = mType;
    if (mType == dom::mozPacketDumpType::Srtp &&
        packet.type() == MediaPacket::RTCP) {
      dumpType = dom::mozPacketDumpType::Srtcp;
    }

    mPacketDumper->Dump(*packet.sdp_level(),
                        dumpType,
                        true,
                        packet.data(),
                        packet.len());
  }
  return downward_->SendPacket(packet);
}

void
TransportLayerPacketDumper::StateChange(TransportLayer* aLayer, State aState)
{
  TL_SET_STATE(aState);
}

void
TransportLayerPacketDumper::PacketReceived(TransportLayer* aLayer,
                                           MediaPacket& packet)
{
  // There's no way to know the level yet, so we can't use the packet dumper
  // yet. We rely on the SRTP layer saving the encrypted packet in
  // MediaPacket::encrypted_, to allow MediaPipeline to dump it later.
  SignalPacketReceived(this, packet);
}

} // namespace mozilla



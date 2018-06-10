/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "logging.h"
#include "transportflow.h"
#include "transportlayerlog.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

void TransportLayerLogging::WasInserted() {
  if (downward_) {
    downward_->SignalStateChange.connect(
        this, &TransportLayerLogging::StateChange);
    downward_->SignalPacketReceived.connect(
        this, &TransportLayerLogging::PacketReceived);
    TL_SET_STATE(downward_->state());
  }
}

TransportResult
TransportLayerLogging::SendPacket(MediaPacket& packet) {
  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "SendPacket(" << packet.len() << ")");

  if (downward_) {
    return downward_->SendPacket(packet);
  }
  return static_cast<TransportResult>(packet.len());
}

void TransportLayerLogging::StateChange(TransportLayer *layer, State state) {
  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Received StateChange to " << state);

  TL_SET_STATE(state);
}

void TransportLayerLogging::PacketReceived(TransportLayer* layer,
                                           MediaPacket& packet) {
  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "PacketReceived(" << packet.len() << ")");

  SignalPacketReceived(this, packet);
}

}  // close namespace

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef transportlayerlog_h__
#define transportlayerlog_h__

#include "m_cpp_utils.h"
#include "transportflow.h"
#include "transportlayer.h"

namespace mozilla {

class TransportLayerLogging : public TransportLayer {
public:
  TransportLayerLogging() {}

  // Overrides for TransportLayer
  TransportResult SendPacket(MediaPacket& packet) override;

  // Signals (forwarded to upper layer)
  void StateChange(TransportLayer *layer, State state);
  void PacketReceived(TransportLayer* layer, MediaPacket& packet);

  TRANSPORT_LAYER_ID("log")

protected:
  void WasInserted() override;

private:
  DISALLOW_COPY_ASSIGN(TransportLayerLogging);
};


}  // close namespace
#endif

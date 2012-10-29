/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com
#include <deque>

#include "transportflow.h"
#include "transportlayer.h"

namespace mozilla {

TransportFlow::~TransportFlow() {
  for (std::deque<TransportLayer *>::iterator it = layers_.begin();
       it != layers_.end(); ++it) {
    delete *it;
  }
}

nsresult TransportFlow::PushLayer(TransportLayer *layer) {
  nsresult rv = layer->Init();
  if (!NS_SUCCEEDED(rv))
    return rv;

  TransportLayer *old_layer = layers_.empty() ? nullptr : layers_.front();

  // Re-target my signals to the new layer
  if (old_layer) {
    old_layer->SignalStateChange.disconnect(this);
    old_layer->SignalPacketReceived.disconnect(this);
  }
  layer->SignalStateChange.connect(this, &TransportFlow::StateChange);
  layer->SignalPacketReceived.connect(this, &TransportFlow::PacketReceived);

  layers_.push_front(layer);
  layer->Inserted(this, old_layer);
  return NS_OK;
}

nsresult TransportFlow::PushLayers(std::queue<TransportLayer *> layers) {
  nsresult rv;

  while (!layers.empty()) {
    rv = PushLayer(layers.front());
    layers.pop();

    if (NS_FAILED(rv)) {
      // Destroy any layers we could not push.
      while (!layers.empty()) {
        delete layers.front();
        layers.pop();
      }

      return rv;
    }
  }

  return NS_OK;
}

TransportLayer *TransportFlow::top() const {
  return layers_.empty() ? nullptr : layers_.front();
}

TransportLayer *TransportFlow::GetLayer(const std::string& id) const {
  for (std::deque<TransportLayer *>::const_iterator it = layers_.begin();
       it != layers_.end(); ++it) {
    if ((*it)->id() == id)
      return *it;
  }

  return nullptr;
}

TransportLayer::State TransportFlow::state() {
  return top() ? top()->state() : TransportLayer::TS_NONE;
}

TransportResult TransportFlow::SendPacket(const unsigned char *data,
                                          size_t len) {
  return top() ? top()->SendPacket(data, len) : TE_ERROR;
}

void TransportFlow::StateChange(TransportLayer *layer,
                                TransportLayer::State state) {
  SignalStateChange(this, state);
}

void TransportFlow::PacketReceived(TransportLayer* layer,
                                   const unsigned char *data,
                                   size_t len) {
  SignalPacketReceived(this, data, len);
}






}  // close namespace

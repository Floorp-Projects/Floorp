/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com
#include <deque>

#include <prlog.h>

#include "logging.h"
#include "transportflow.h"
#include "transportlayer.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

TransportFlow::~TransportFlow() {
  for (std::deque<TransportLayer *>::iterator it = layers_.begin();
       it != layers_.end(); ++it) {
    delete *it;
  }
}

nsresult TransportFlow::PushLayer(TransportLayer *layer) {
  // Don't allow pushes once we are in error state.
  if (state_ == TransportLayer::TS_ERROR) {
    MOZ_MTLOG(PR_LOG_ERROR, id_ + ": Can't call PushLayer in error state for flow ");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = layer->Init();
  if (!NS_SUCCEEDED(rv)) {
    // Set ourselves to have failed.
    MOZ_MTLOG(PR_LOG_ERROR, id_ << ": Layer initialization failed; invalidating");
    StateChangeInt(TransportLayer::TS_ERROR);
    return rv;
  }

  TransportLayer *old_layer = layers_.empty() ? nullptr : layers_.front();

  // Re-target my signals to the new layer
  if (old_layer) {
    old_layer->SignalStateChange.disconnect(this);
    old_layer->SignalPacketReceived.disconnect(this);
  }
  layers_.push_front(layer);
  layer->Inserted(this, old_layer);

  layer->SignalStateChange.connect(this, &TransportFlow::StateChange);
  layer->SignalPacketReceived.connect(this, &TransportFlow::PacketReceived);
  StateChangeInt(layer->state());

  return NS_OK;
}

// This is all-or-nothing.
nsresult TransportFlow::PushLayers(nsAutoPtr<std::queue<TransportLayer *> > layers) {
  MOZ_ASSERT(!layers->empty());
  if (layers->empty()) {
    MOZ_MTLOG(PR_LOG_ERROR, id_ << ": Can't call PushLayers with empty layers");
    return NS_ERROR_INVALID_ARG;
  }

  // Don't allow pushes once we are in error state.
  if (state_ == TransportLayer::TS_ERROR) {
    MOZ_MTLOG(PR_LOG_ERROR, id_ << ": Can't call PushLayers in error state for flow ");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  // Disconnect all the old signals.
  disconnect_all();

  TransportLayer *layer;

  while (!layers->empty()) {
    TransportLayer *old_layer = layers_.empty() ? nullptr : layers_.front();
    layer = layers->front();

    rv = layer->Init();
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(PR_LOG_ERROR, id_ << ": Layer initialization failed; invalidating flow ");
      break;
    }

    // Push the layer onto the queue.
    layers_.push_front(layer);
    layers->pop();
    layer->Inserted(this, old_layer);
  }

  if (NS_FAILED(rv)) {
    // Destroy any layers we could not push.
    while (!layers->empty()) {
      delete layers->front();
      layers->pop();
    }

    // Now destroy the rest of the flow, because it's no longer
    // in an acceptable state.
    while (!layers_.empty()) {
      delete layers_.front();
      layers_.pop_front();
    }

    // Set ourselves to have failed.
    StateChangeInt(TransportLayer::TS_ERROR);

    // Return failure.
    return rv;
  }

  // Finally, attach ourselves to the top layer.
  layer->SignalStateChange.connect(this, &TransportFlow::StateChange);
  layer->SignalPacketReceived.connect(this, &TransportFlow::PacketReceived);
  StateChangeInt(layer->state());  // Signals if the state changes.

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
  return state_;
}

TransportResult TransportFlow::SendPacket(const unsigned char *data,
                                          size_t len) {
  if (state_ != TransportLayer::TS_OPEN) {
    return TE_ERROR;
  }
  return top() ? top()->SendPacket(data, len) : TE_ERROR;
}

void TransportFlow::StateChangeInt(TransportLayer::State state) {
  if (state == state_) {
    return;
  }

  state_ = state;
  SignalStateChange(this, state_);
}

void TransportFlow::StateChange(TransportLayer *layer,
                                TransportLayer::State state) {
  StateChangeInt(state);
}

void TransportFlow::PacketReceived(TransportLayer* layer,
                                   const unsigned char *data,
                                   size_t len) {
  SignalPacketReceived(this, data, len);
}

}  // close namespace

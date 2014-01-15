/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com
#include <deque>

#include "logging.h"
#include "runnable_utils.h"
#include "transportflow.h"
#include "transportlayer.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

// There are some hacks here to allow destruction off of
// the main thread.
TransportFlow::~TransportFlow() {
  // Make sure that if we are off the right thread, we have
  // no more attached signals.
  if (!CheckThreadInt()) {
    MOZ_ASSERT(SignalStateChange.is_empty());
    MOZ_ASSERT(SignalPacketReceived.is_empty());
  }

  // Push the destruction onto the STS thread. Note that there
  // is still some possibility that someone is accessing this
  // object simultaneously, but as long as smart pointer discipline
  // is maintained, it shouldn't be possible to access and
  // destroy it simultaneously. The conversion to an nsAutoPtr
  // ensures automatic destruction of the queue at exit of
  // DestroyFinal.
  nsAutoPtr<std::deque<TransportLayer*> > layers_tmp(layers_.forget());
  RUN_ON_THREAD(target_,
                WrapRunnableNM(&TransportFlow::DestroyFinal, layers_tmp),
                NS_DISPATCH_NORMAL);
}

void TransportFlow::DestroyFinal(nsAutoPtr<std::deque<TransportLayer *> > layers) {
  ClearLayers(layers);
}

void TransportFlow::ClearLayers(std::queue<TransportLayer *>* layers) {
  while (!layers->empty()) {
    delete layers->front();
    layers->pop();
  }
}

void TransportFlow::ClearLayers(std::deque<TransportLayer *>* layers) {
  while (!layers->empty()) {
    delete layers->front();
    layers->pop_front();
  }
}

nsresult TransportFlow::PushLayer(TransportLayer *layer) {
  CheckThread();
  ScopedDeletePtr<TransportLayer> layer_tmp(layer);  // Destroy on failure.

  // Don't allow pushes once we are in error state.
  if (state_ == TransportLayer::TS_ERROR) {
    MOZ_MTLOG(ML_ERROR, id_ + ": Can't call PushLayer in error state for flow");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = layer->Init();
  if (!NS_SUCCEEDED(rv)) {
    // Destroy the rest of the flow, because it's no longer in an acceptable
    // state.
    ClearLayers(layers_.get());

    // Set ourselves to have failed.
    MOZ_MTLOG(ML_ERROR, id_ << ": Layer initialization failed; invalidating");
    StateChangeInt(TransportLayer::TS_ERROR);

    return rv;
  }
  EnsureSameThread(layer);

  TransportLayer *old_layer = layers_->empty() ? nullptr : layers_->front();

  // Re-target my signals to the new layer
  if (old_layer) {
    old_layer->SignalStateChange.disconnect(this);
    old_layer->SignalPacketReceived.disconnect(this);
  }
  layers_->push_front(layer_tmp.forget());
  layer->Inserted(this, old_layer);

  layer->SignalStateChange.connect(this, &TransportFlow::StateChange);
  layer->SignalPacketReceived.connect(this, &TransportFlow::PacketReceived);
  StateChangeInt(layer->state());

  return NS_OK;
}

// This is all-or-nothing.
nsresult TransportFlow::PushLayers(nsAutoPtr<std::queue<TransportLayer *> > layers) {
  CheckThread();

  MOZ_ASSERT(!layers->empty());
  if (layers->empty()) {
    MOZ_MTLOG(ML_ERROR, id_ << ": Can't call PushLayers with empty layers");
    return NS_ERROR_INVALID_ARG;
  }

  // Don't allow pushes once we are in error state.
  if (state_ == TransportLayer::TS_ERROR) {
    MOZ_MTLOG(ML_ERROR,
              id_ << ": Can't call PushLayers in error state for flow ");
    ClearLayers(layers.get());
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  // Disconnect all the old signals.
  disconnect_all();

  TransportLayer *layer;

  while (!layers->empty()) {
    TransportLayer *old_layer = layers_->empty() ? nullptr : layers_->front();
    layer = layers->front();

    rv = layer->Init();
    if (NS_FAILED(rv)) {
      MOZ_MTLOG(ML_ERROR,
                id_ << ": Layer initialization failed; invalidating flow ");
      break;
    }

    EnsureSameThread(layer);

    // Push the layer onto the queue.
    layers_->push_front(layer);
    layers->pop();
    layer->Inserted(this, old_layer);
  }

  if (NS_FAILED(rv)) {
    // Destroy any layers we could not push.
    ClearLayers(layers);

    // Now destroy the rest of the flow, because it's no longer
    // in an acceptable state.
    ClearLayers(layers_);

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
  CheckThread();

  return layers_->empty() ? nullptr : layers_->front();
}

TransportLayer *TransportFlow::GetLayer(const std::string& id) const {
  CheckThread();

  for (std::deque<TransportLayer *>::const_iterator it = layers_->begin();
       it != layers_->end(); ++it) {
    if ((*it)->id() == id)
      return *it;
  }

  return nullptr;
}

TransportLayer::State TransportFlow::state() {
  CheckThread();

  return state_;
}

TransportResult TransportFlow::SendPacket(const unsigned char *data,
                                          size_t len) {
  CheckThread();

  if (state_ != TransportLayer::TS_OPEN) {
    return TE_ERROR;
  }
  return top() ? top()->SendPacket(data, len) : TE_ERROR;
}

bool TransportFlow::Contains(TransportLayer *layer) const {
  if (layers_) {
    for (auto l = layers_->begin(); l != layers_->end(); ++l) {
      if (*l == layer) {
        return true;
      }
    }
  }
  return false;
}

void TransportFlow::EnsureSameThread(TransportLayer *layer)  {
  // Enforce that if any of the layers have a thread binding,
  // they all have the same binding.
  if (target_) {
    const nsCOMPtr<nsIEventTarget>& lthread = layer->GetThread();

    if (lthread && (lthread != target_))
      MOZ_CRASH();
  }
  else {
    target_ = layer->GetThread();
  }
}

void TransportFlow::StateChangeInt(TransportLayer::State state) {
  CheckThread();

  if (state == state_) {
    return;
  }

  state_ = state;
  SignalStateChange(this, state_);
}

void TransportFlow::StateChange(TransportLayer *layer,
                                TransportLayer::State state) {
  CheckThread();

  StateChangeInt(state);
}

void TransportFlow::PacketReceived(TransportLayer* layer,
                                   const unsigned char *data,
                                   size_t len) {
  CheckThread();

  SignalPacketReceived(this, data, len);
}

}  // close namespace

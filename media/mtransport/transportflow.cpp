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

NS_IMPL_ISUPPORTS0(TransportFlow)

// There are some hacks here to allow destruction off of
// the main thread.
TransportFlow::~TransportFlow() {
  // Push the destruction onto the STS thread. Note that there
  // is still some possibility that someone is accessing this
  // object simultaneously, but as long as smart pointer discipline
  // is maintained, it shouldn't be possible to access and
  // destroy it simultaneously. The conversion to an nsAutoPtr
  // ensures automatic destruction of the queue at exit of
  // DestroyFinal.
  if (target_) {
    nsAutoPtr<std::deque<TransportLayer*>> layers_tmp(layers_.release());
    RUN_ON_THREAD(target_,
                  WrapRunnableNM(&TransportFlow::DestroyFinal, layers_tmp),
                  NS_DISPATCH_NORMAL);
  }
}

void TransportFlow::DestroyFinal(nsAutoPtr<std::deque<TransportLayer *> > layers) {
  ClearLayers(layers.get());
}

void TransportFlow::ClearLayers(std::deque<TransportLayer *>* layers) {
  while (!layers->empty()) {
    delete layers->front();
    layers->pop_front();
  }
}

void TransportFlow::PushLayer(TransportLayer* layer) {
  CheckThread();
  layers_->push_front(layer);
  EnsureSameThread(layer);
  layer->SetFlowId(id_);
}

TransportLayer *TransportFlow::GetLayer(const std::string& id) const {
  CheckThread();

  if (layers_) {
    for (TransportLayer* layer : *layers_) {
      if (layer->id() == id)
        return layer;
    }
  }

  return nullptr;
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

}  // close namespace

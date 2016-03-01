/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef transportflow_h__
#define transportflow_h__

#include <deque>
#include <queue>
#include <string>

#include "nscore.h"
#include "nsISupportsImpl.h"
#include "mozilla/UniquePtr.h"
#include "transportlayer.h"
#include "m_cpp_utils.h"
#include "nsAutoPtr.h"

// A stack of transport layers acts as a flow.
// Generally, one reads and writes to the top layer.

// This code has a confusing hybrid threading model which
// probably needs some eventual refactoring.
// TODO(ekr@rtfm.com): Bug 844891
//
// TransportFlows are not inherently bound to a thread *but*
// TransportLayers can be. If any layer in a flow is bound
// to a given thread, then all layers in the flow MUST be
// bound to that thread and you can only manipulate the
// flow (push layers, write, etc.) on that thread.
//
// The sole official exception to this is that you are
// allowed to *destroy* a flow off the bound thread provided
// that there are no listeners on its signals. This exception
// is designed to allow idioms where you create the flow
// and then something goes wrong and you destroy it and
// you don't want to bother with a thread dispatch.
//
// Eventually we hope to relax the "no listeners"
// restriction by thread-locking the signals, but previous
// attempts have caused deadlocks.
//
// Most of these invariants are enforced by hard asserts
// (i.e., those which fire even in production builds).

namespace mozilla {

class TransportFlow final : public nsISupports,
                            public sigslot::has_slots<> {
 public:
  TransportFlow()
    : id_("(anonymous)"),
      state_(TransportLayer::TS_NONE),
      layers_(new std::deque<TransportLayer *>) {}
  explicit TransportFlow(const std::string id)
    : id_(id),
      state_(TransportLayer::TS_NONE),
      layers_(new std::deque<TransportLayer *>) {}

  const std::string& id() const { return id_; }

  // Layer management. Note PushLayer() is not thread protected, so
  // either:
  // (a) Do it in the thread handling the I/O
  // (b) Do it before you activate the I/O system
  //
  // The flow takes ownership of the layers after a successful
  // push.
  nsresult PushLayer(TransportLayer *layer);

  // Convenience function to push multiple layers on. Layers
  // are pushed on in the order that they are in the queue.
  // Any failures cause the flow to become inoperable and
  // destroys all the layers including those already pushed.
  // TODO(ekr@rtfm.com): Change layers to be ref-counted.
  nsresult PushLayers(nsAutoPtr<std::queue<TransportLayer *> > layers);

  TransportLayer *top() const;
  TransportLayer *GetLayer(const std::string& id) const;

  // Wrappers for whatever TLayer happens to be the top layer
  // at the time. This way you don't need to do top()->Foo().
  TransportLayer::State state(); // Current state
  TransportResult SendPacket(const unsigned char *data, size_t len);

  // State has changed. Reflects the top flow.
  sigslot::signal2<TransportFlow *, TransportLayer::State>
    SignalStateChange;

  // Data received on the flow
  sigslot::signal3<TransportFlow*, const unsigned char *, size_t>
    SignalPacketReceived;

  bool Contains(TransportLayer *layer) const;

  NS_DECL_THREADSAFE_ISUPPORTS

 private:
  ~TransportFlow();

  DISALLOW_COPY_ASSIGN(TransportFlow);

  // Check if we are on the right thread
  void CheckThread() const {
    if (!CheckThreadInt())
      MOZ_CRASH();
  }

  bool CheckThreadInt() const {
    bool on;

    if (!target_)  // OK if no thread set.
      return true;
    if (NS_FAILED(target_->IsOnCurrentThread(&on)))
      return false;

    return on;
  }

  void EnsureSameThread(TransportLayer *layer);

  void StateChange(TransportLayer *layer, TransportLayer::State state);
  void StateChangeInt(TransportLayer::State state);
  void PacketReceived(TransportLayer* layer, const unsigned char *data,
      size_t len);
  static void DestroyFinal(nsAutoPtr<std::deque<TransportLayer *> > layers);

  // Overload needed because we use deque internally and queue externally.
  static void ClearLayers(std::deque<TransportLayer *>* layers);
  static void ClearLayers(std::queue<TransportLayer *>* layers);

  std::string id_;
  TransportLayer::State state_;
  UniquePtr<std::deque<TransportLayer *>> layers_;
  nsCOMPtr<nsIEventTarget> target_;
};

}  // close namespace
#endif

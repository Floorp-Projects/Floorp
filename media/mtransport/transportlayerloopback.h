/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef transportlayerloopback_h__
#define transportlayerloopback_h__

#include "nspr.h"
#include "prio.h"
#include "prlock.h"

#include <memory>
#include <queue>


#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsINamed.h"
#include "nsITimer.h"


#include "m_cpp_utils.h"
#include "transportflow.h"
#include "transportlayer.h"

// A simple loopback transport layer that is used for testing.
namespace mozilla {

class TransportLayerLoopback : public TransportLayer {
 public:
  TransportLayerLoopback() :
      peer_(nullptr),
      timer_(nullptr),
      packets_(),
      packets_lock_(nullptr),
      deliverer_(nullptr),
      combinePackets_(false) {}

  ~TransportLayerLoopback() {
    while (!packets_.empty()) {
      QueuedPacket *packet = packets_.front();
      packets_.pop();
      delete packet;
    }
    if (packets_lock_) {
      PR_DestroyLock(packets_lock_);
    }
    timer_->Cancel();
    deliverer_->Detach();
  }

  // Init
  nsresult Init();

  // Connect to the other side
  void Connect(TransportLayerLoopback* peer);

  // Disconnect
  void Disconnect() {
    TransportLayerLoopback *peer = peer_;

    peer_ = nullptr;
    if (peer) {
      peer->Disconnect();
    }
  }

  void CombinePackets(bool combine) { combinePackets_ = combine; }

  // Overrides for TransportLayer
  TransportResult SendPacket(const unsigned char *data, size_t len) override;

  // Deliver queued packets
  void DeliverPackets();

  TRANSPORT_LAYER_ID("loopback")

 private:
  DISALLOW_COPY_ASSIGN(TransportLayerLoopback);

  // A queued packet
  class QueuedPacket {
   public:
    QueuedPacket() : data_(nullptr), len_(0) {}
    ~QueuedPacket() {
      delete [] data_;
    }

    void Assign(const unsigned char *data, size_t len) {
      data_ = new unsigned char[len];
      memcpy(static_cast<void *>(data_),
             static_cast<const void *>(data), len);
      len_ = len;
    }

    void Assign(const unsigned char *data1, size_t len1,
                const unsigned char *data2, size_t len2) {
      data_ = new unsigned char[len1 + len2];
      memcpy(static_cast<void *>(data_),
             static_cast<const void *>(data1), len1);
      memcpy(static_cast<void *>(data_ + len1),
             static_cast<const void *>(data2), len2);
      len_ = len1 + len2;
    }

    const unsigned char *data() const { return data_; }
    size_t len() const { return len_; }

   private:
    DISALLOW_COPY_ASSIGN(QueuedPacket);

    unsigned char *data_;
    size_t len_;
  };

  // A timer to deliver packets if some are available
  // Fires every 100 ms
  class Deliverer : public nsITimerCallback
                  , public nsINamed {
   public:
    explicit Deliverer(TransportLayerLoopback *layer) :
        layer_(layer) {}
    void Detach() {
      layer_ = nullptr;
    }

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSITIMERCALLBACK
    NS_DECL_NSINAMED

 private:
    virtual ~Deliverer() {
    }

    DISALLOW_COPY_ASSIGN(Deliverer);

    TransportLayerLoopback *layer_;
  };

  // Queue a packet for delivery
  nsresult QueuePacket(const unsigned char *data, size_t len);

  TransportLayerLoopback* peer_;
  nsCOMPtr<nsITimer> timer_;
  std::queue<QueuedPacket *> packets_;
  PRLock *packets_lock_;
  RefPtr<Deliverer> deliverer_;
  bool combinePackets_;
};

}  // close namespace
#endif

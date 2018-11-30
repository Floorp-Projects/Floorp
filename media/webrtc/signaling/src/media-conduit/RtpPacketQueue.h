/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtpPacketQueue_h
#define RtpPacketQueue_h

#include "MediaConduitInterface.h"
#include "nsTArray.h"

namespace mozilla {

class RtpPacketQueue {
 public:
  void Clear() {
    mQueuedPackets.Clear();
    mQueueActive = false;
  }

  void DequeueAll(MediaSessionConduit* conduit) {
    // SSRC is set; insert queued packets
    for (auto& packet : mQueuedPackets) {
      if (conduit->DeliverPacket(packet->mData, packet->mLen) !=
          kMediaConduitNoError) {
        // Keep delivering and then clear the queue
      }
    }
    mQueuedPackets.Clear();
    mQueueActive = false;
  }

  void Enqueue(const void* data, int len) {
    UniquePtr<QueuedPacket> packet(new QueuedPacket(data, len));
    mQueuedPackets.AppendElement(std::move(packet));
    mQueueActive = true;
  }

  bool IsQueueActive() { return mQueueActive; }

 private:
  bool mQueueActive = false;
  struct QueuedPacket {
    const int mLen;
    uint8_t* mData;

    QueuedPacket(const void* aData, size_t aLen) : mLen(aLen) {
      mData = new uint8_t[mLen];
      memcpy(mData, aData, mLen);
    }

    ~QueuedPacket() { delete (mData); }
  };
  nsTArray<UniquePtr<QueuedPacket>> mQueuedPackets;
};

}  // namespace mozilla

#endif  // RtpPacketQueue_h

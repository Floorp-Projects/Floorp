/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RTP_PACKET_QUEUE_H_
#define RTP_PACKET_QUEUE_H_

#include "nsTArray.h"

#include "MediaConduitInterface.h"

namespace mozilla {

class RtpPacketQueue {
public:

  void Clear()
  {
    mQueuedPackets.Clear();
    mQueueActive = false;
  }

  void DequeueAll(MediaSessionConduit* conduit)
  {
    // SSRC is set; insert queued packets
    for (auto& packet : mQueuedPackets) {
      if (conduit->DeliverPacket(packet->mData, packet->mLen) != kMediaConduitNoError) {
        // Keep delivering and then clear the queue
      }
    }
    mQueuedPackets.Clear();
    mQueueActive = false;
  }

  void Enqueue(const void* data, int len)
  {
    UniquePtr<QueuedPacket> packet((QueuedPacket*) malloc(sizeof(QueuedPacket) + len-1));
    packet->mLen = len;
    memcpy(packet->mData, data, len);
    mQueuedPackets.AppendElement(std::move(packet));
    mQueueActive = true;
  }

  bool IsQueueActive()
  {
    return mQueueActive;
  }

private:
  bool mQueueActive = false;
  struct QueuedPacket {
    int mLen;
    uint8_t mData[1];
  };
  nsTArray<UniquePtr<QueuedPacket>> mQueuedPackets;
};

}

#endif

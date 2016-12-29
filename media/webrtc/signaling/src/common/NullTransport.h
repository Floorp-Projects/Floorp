/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef NULL_TRANSPORT_H_
#define NULL_TRANSPORT_H_

#include "mozilla/Attributes.h"

#include "webrtc/transport.h"

namespace mozilla {

/**
 * NullTransport is registered as ExternalTransport to throw away data
 */
class NullTransport : public webrtc::Transport
{
public:
  virtual bool SendRtp(const uint8_t* packet,
                       size_t length,
                       const webrtc::PacketOptions& options) override
  {
    (void) packet;
    (void) length;
    (void) options;
    return true;
  }

  virtual bool SendRtcp(const uint8_t* packet, size_t length) override
  {
    (void) packet;
    (void) length;
    return true;
  }
#if 0
  virtual int SendPacket(int channel, const void *data, size_t len)
  {
    (void) channel; (void) data;
    return len;
  }

  virtual int SendRTCPPacket(int channel, const void *data, size_t len)
  {
    (void) channel; (void) data;
    return len;
  }
#endif
  NullTransport() {}

  virtual ~NullTransport() {}

private:
  NullTransport(const NullTransport& other) = delete;
  void operator=(const NullTransport& other) = delete;
};

} // end namespace

#endif

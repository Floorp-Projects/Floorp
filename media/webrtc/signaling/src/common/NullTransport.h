/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef NULL_TRANSPORT_H_
#define NULL_TRANSPORT_H_

#include "mozilla/Attributes.h"

#include "common_types.h"

namespace mozilla {

/**
 * NullTransport is registered as ExternalTransport to throw away data
 */
class NullTransport : public webrtc::Transport
{
public:
  virtual int SendPacket(int channel, const void *data, int len)
  {
    (void) channel; (void) data;
    return len;
  }

  virtual int SendRTCPPacket(int channel, const void *data, int len)
  {
    (void) channel; (void) data;
    return len;
  }

  NullTransport() {}

  virtual ~NullTransport() {}

private:
  NullTransport(const NullTransport& other) MOZ_DELETE;
  void operator=(const NullTransport& other) MOZ_DELETE;
};

} // end namespace

#endif

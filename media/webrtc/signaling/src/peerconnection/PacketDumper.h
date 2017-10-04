/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _PACKET_DUMPER_H_
#define _PACKET_DUMPER_H_

#include "nsISupportsImpl.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/RTCPeerConnectionBinding.h"

namespace mozilla {
class PeerConnectionImpl;

class PacketDumper
{
  public:
    explicit PacketDumper(PeerConnectionImpl* aPc);
    explicit PacketDumper(const std::string& aPcHandle);
    PacketDumper(const PacketDumper&) = delete;
    ~PacketDumper();

    PacketDumper& operator=(const PacketDumper&) = delete;

    void Dump(size_t level, dom::mozPacketDumpType type, bool sending,
              const void* data, size_t size);

  private:
    RefPtr<PeerConnectionImpl> mPc;
};

} // namespace mozilla

#endif // _PACKET_DUMPER_H_


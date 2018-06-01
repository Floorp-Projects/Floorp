/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/peerconnection/PacketDumper.h"
#include "signaling/src/peerconnection/PeerConnectionImpl.h"
#include "mozilla/media/MediaUtils.h" // NewRunnableFrom
#include "nsThreadUtils.h" // NS_DispatchToMainThread

namespace mozilla {

PacketDumper::PacketDumper(PeerConnectionImpl* aPc) :
  mPc(aPc)
{
}

PacketDumper::PacketDumper(const std::string& aPcHandle)
{
  if (!aPcHandle.empty()) {
    PeerConnectionWrapper pcw(aPcHandle);
    mPc = pcw.impl();
  }
}

PacketDumper::~PacketDumper()
{
  RefPtr<Runnable> pcDisposeRunnable =
    media::NewRunnableFrom(
        std::bind(
          [](RefPtr<PeerConnectionImpl> pc) {
            return NS_OK;
          },
          mPc.forget()));
  NS_DispatchToMainThread(pcDisposeRunnable);
}

void
PacketDumper::Dump(size_t level, dom::mozPacketDumpType type, bool sending,
                   const void* data, size_t size)
{
  // Optimization; avoids making a copy of the buffer, but we need to lock a
  // mutex and check the flags. Could be optimized further, if we really want to
  if (!mPc || !mPc->ShouldDumpPacket(level, type, sending)) {
    return;
  }

  RefPtr<PeerConnectionImpl> pc = mPc;

  UniquePtr<uint8_t[]> ownedPacket = MakeUnique<uint8_t[]>(size);
  memcpy(ownedPacket.get(), data, size);

  RefPtr<Runnable> dumpRunnable =
    media::NewRunnableFrom(
        std::bind(
          [pc, level, type, sending, size](UniquePtr<uint8_t[]>& packet)
            -> nsresult
          {
            pc->DumpPacket_m(level, type, sending, packet, size);
            return NS_OK;
          },
          std::move(ownedPacket)));

  NS_DispatchToMainThread(dumpRunnable);
}

} //namespace mozilla


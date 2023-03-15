/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProtocolFuzzer_h
#define mozilla_ipc_ProtocolFuzzer_h

#include "chrome/common/ipc_message.h"

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace ipc {

class ProtocolFuzzerHelper {
 public:
  static mozilla::dom::ContentParent* CreateContentParent(
      const nsACString& aRemoteType);

  static void CompositorBridgeParentSetup();

  static void AddShmemToProtocol(IToplevelProtocol* aProtocol,
                                 Shmem::SharedMemory* aSegment, int32_t aId) {
    MOZ_ASSERT(!aProtocol->mShmemMap.Contains(aId),
               "Don't insert with an existing ID");
    aProtocol->mShmemMap.InsertOrUpdate(aId, aSegment);
  }

  static void RemoveShmemFromProtocol(IToplevelProtocol* aProtocol,
                                      int32_t aId) {
    aProtocol->mShmemMap.Remove(aId);
  }
};

template <typename T>
void FuzzProtocol(T* aProtocol, const uint8_t* aData, size_t aSize,
                  const nsTArray<nsCString>& aIgnoredMessageTypes) {
  while (true) {
    uint32_t msg_size =
        IPC::Message::MessageSize(reinterpret_cast<const char*>(aData),
                                  reinterpret_cast<const char*>(aData) + aSize);
    if (msg_size == 0 || msg_size > aSize) {
      break;
    }
    IPC::Message m(reinterpret_cast<const char*>(aData), msg_size);
    aSize -= msg_size;
    aData += msg_size;

    // We ignore certain message types
    if (aIgnoredMessageTypes.Contains(m.name())) {
      continue;
    }

    uint8_t num_shmems = 0;
    if (aSize) {
      num_shmems = *aData;
      aData++;
      aSize--;

      for (uint32_t i = 0; i < num_shmems; i++) {
        if (aSize < sizeof(uint16_t)) {
          break;
        }
        size_t shmem_size = *reinterpret_cast<const uint16_t*>(aData);
        aData += sizeof(uint16_t);
        aSize -= sizeof(uint16_t);

        if (shmem_size > aSize) {
          break;
        }
        RefPtr<Shmem::SharedMemory> segment(Shmem::Alloc(shmem_size, false));
        if (!segment) {
          break;
        }

        Shmem shmem(segment.get(), i + 1);
        memcpy(shmem.get<uint8_t>(), aData, shmem_size);
        ProtocolFuzzerHelper::AddShmemToProtocol(
            aProtocol, segment.forget().take(), i + 1);

        aData += shmem_size;
        aSize -= shmem_size;
      }
    }
    // TODO: attach |m.header().num_fds| file descriptors to |m|. MVP can be
    // empty files, next implementation maybe read a length header from |data|
    // and then that many bytes.

    if (m.is_sync()) {
      UniquePtr<IPC::Message> reply;
      aProtocol->OnMessageReceived(m, reply);
    } else {
      aProtocol->OnMessageReceived(m);
    }
    for (uint32_t i = 0; i < num_shmems; i++) {
      Shmem::SharedMemory* segment = aProtocol->LookupSharedMemory(i + 1);
      Shmem::Dealloc(segment);
      ProtocolFuzzerHelper::RemoveShmemFromProtocol(aProtocol, i + 1);
    }
  }
}

nsTArray<nsCString> LoadIPCMessageBlacklist(const char* aPath);

}  // namespace ipc
}  // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_ProtocolFuzzer_h
#define mozilla_ipc_ProtocolFuzzer_h

#include "chrome/common/ipc_message.h"

#include "mozilla/RefPtr.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace ipc {

template<typename T>
void
FuzzProtocol(T* aProtocol,
             const uint8_t* aData,
             size_t aSize,
             const nsTArray<nsCString>& aIgnoredMessageTypes)
{
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
    // TODO: attach |m.header().num_fds| file descriptors to |m|. MVP can be
    // empty files, next implementation maybe read a length header from |data|
    // and then that many bytes. Probably need similar handling for shmem,
    // other types?

    if (m.is_sync()) {
      nsAutoPtr<IPC::Message> reply;
      aProtocol->OnMessageReceived(m, *getter_Transfers(reply));
    } else {
      aProtocol->OnMessageReceived(m);
    }
  }
}

nsTArray<nsCString> LoadIPCMessageBlacklist(const char* aPath);

class ProtocolFuzzerHelper
{
public:
  static mozilla::dom::ContentParent* CreateContentParent(
    mozilla::dom::ContentParent* aOpener,
    const nsAString& aRemoteType);
};
}
}

#endif

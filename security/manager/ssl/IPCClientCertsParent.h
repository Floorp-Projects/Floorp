/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_psm_IPCClientCertsParent_h__
#define mozilla_psm_IPCClientCertsParent_h__

#include "mozilla/psm/PIPCClientCertsParent.h"

namespace mozilla {

namespace net {
class SocketProcessBackgroundParent;
}  // namespace net

namespace psm {

class IPCClientCertsParent final : public PIPCClientCertsParent {
  friend class mozilla::net::SocketProcessBackgroundParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IPCClientCertsParent)

  mozilla::ipc::IPCResult RecvFindObjects(
      nsTArray<IPCClientCertObject>* aObjects);
  mozilla::ipc::IPCResult RecvSign(ByteArray aCert, ByteArray aData,
                                   ByteArray aParams, ByteArray* aSignature);

 private:
  IPCClientCertsParent();
  ~IPCClientCertsParent() = default;
};

}  // namespace psm
}  // namespace mozilla

#endif

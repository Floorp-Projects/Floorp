/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SECURITY_MANAGER_SSL_SELECTTLSCLIENTAUTHCERTCHILD_H_
#define SECURITY_MANAGER_SSL_SELECTTLSCLIENTAUTHCERTCHILD_H_

#include "mozilla/psm/PSelectTLSClientAuthCertChild.h"
#include "TLSClientAuthCertSelection.h"

namespace mozilla {
namespace psm {

// Socket process component of the SelectTLSClientAuthCert IPC protocol. When
// the parent process selects a client authentication certificate (or opts for
// no certificate), RecvTLSClientAuthCertSelected will be called via IPC with
// the bytes of the certificate (and the bytes of the associated certificate
// chain). That function dispatches an event to the socket thread that notifies
// NSS that the associated connection can continue.
class SelectTLSClientAuthCertChild : public PSelectTLSClientAuthCertChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SelectTLSClientAuthCertChild, override)

  explicit SelectTLSClientAuthCertChild(
      ClientAuthCertificateSelected* continuation);

  ipc::IPCResult RecvTLSClientAuthCertSelected(
      ByteArray&& aSelectedCertBytes,
      nsTArray<ByteArray>&& aSelectedCertChainBytes);

 private:
  ~SelectTLSClientAuthCertChild() = default;

  RefPtr<ClientAuthCertificateSelected> mContinuation;
};

}  // namespace psm
}  // namespace mozilla

#endif  // SECURITY_MANAGER_SSL_SELECTTLSCLIENTAUTHCERTCHILD_H_

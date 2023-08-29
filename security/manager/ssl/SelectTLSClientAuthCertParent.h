/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SECURITY_MANAGER_SSL_SELECTTLSCLIENTAUTHCERTPARENT_H_
#define SECURITY_MANAGER_SSL_SELECTTLSCLIENTAUTHCERTPARENT_H_

#include "mozilla/OriginAttributes.h"
#include "mozilla/psm/PSelectTLSClientAuthCertParent.h"

namespace mozilla {
namespace psm {

// Parent process component of the SelectTLSClientAuthCert IPC protocol. When
// the socket process encounters a TLS server that requests a client
// authentication certificate, Dispatch will be called via IPC with the
// information associated with that connection. That function dispatches an
// event to the main thread that determines what certificate to select, if any
// (usually by opening a dialog for the user to interact with). When a
// certificate (or no certificate) has been selected, TLSClientAuthCertSelected
// will be called on the IPC thread, which will cause
// SelectTLSClientAuthCertChild::RecvTLSClientAuthCertSelected to be called via
// IPC, which will get the appropriate information to NSS to continue the
// connection.
class SelectTLSClientAuthCertParent : public PSelectTLSClientAuthCertParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SelectTLSClientAuthCertParent, override)

  SelectTLSClientAuthCertParent() = default;

  bool Dispatch(const nsACString& aHostName,
                const OriginAttributes& aOriginAttributes, const int32_t& aPort,
                const uint32_t& aProviderFlags,
                const uint32_t& aProviderTlsFlags,
                const ByteArray& aServerCertBytes,
                nsTArray<ByteArray>&& aCANames,
                const uint64_t& aBrowsingContextID);

  void TLSClientAuthCertSelected(
      const nsTArray<uint8_t>& aSelectedCertBytes,
      nsTArray<nsTArray<uint8_t>>&& aSelectedCertChainBytes);

 private:
  ~SelectTLSClientAuthCertParent() = default;

  void ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason aWhy) override;
};

}  // namespace psm
}  // namespace mozilla

#endif  // SECURITY_MANAGER_SSL_SELECTTLSCLIENTAUTHCERTPARENT_H_

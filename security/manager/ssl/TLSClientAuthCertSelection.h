/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SECURITY_MANAGER_SSL_TLSCLIENTAUTHCERTSELECTION_H_
#define SECURITY_MANAGER_SSL_TLSCLIENTAUTHCERTSELECTION_H_

#include "nsIX509Cert.h"
#include "ssl.h"

namespace mozilla {
class OriginAttributes;
}  // namespace mozilla

// This class is used to store the needed information for invoking the client
// cert selection UI.
class ClientAuthInfo final {
 public:
  explicit ClientAuthInfo(const nsACString& hostName,
                          const mozilla::OriginAttributes& originAttributes,
                          int32_t port, uint32_t providerFlags,
                          uint32_t providerTlsFlags);
  ~ClientAuthInfo() = default;
  ClientAuthInfo(ClientAuthInfo&& aOther) noexcept;

  const nsACString& HostName() const;
  const mozilla::OriginAttributes& OriginAttributesRef() const;
  int32_t Port() const;
  uint32_t ProviderFlags() const;
  uint32_t ProviderTlsFlags() const;

 private:
  ClientAuthInfo(const ClientAuthInfo&) = delete;
  void operator=(const ClientAuthInfo&) = delete;

  nsCString mHostName;
  mozilla::OriginAttributes mOriginAttributes;
  int32_t mPort;
  uint32_t mProviderFlags;
  uint32_t mProviderTlsFlags;
};

SECStatus DoGetClientAuthData(ClientAuthInfo&& info,
                              const mozilla::UniqueCERTCertificate& serverCert,
                              nsTArray<nsTArray<uint8_t>>&& collectedCANames,
                              mozilla::UniqueCERTCertificate& outCert,
                              mozilla::UniqueCERTCertList& outBuiltChain);

SECStatus nsNSS_SSLGetClientAuthData(void* arg, PRFileDesc* socket,
                                     CERTDistNames* caNames,
                                     CERTCertificate** pRetCert,
                                     SECKEYPrivateKey** pRetKey);

#endif  // SECURITY_MANAGER_SSL_TLSCLIENTAUTHCERTSELECTION_H_

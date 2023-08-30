/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SECURITY_MANAGER_SSL_TLSCLIENTAUTHCERTSELECTION_H_
#define SECURITY_MANAGER_SSL_TLSCLIENTAUTHCERTSELECTION_H_

#include "NSSSocketControl.h"
#include "nsIX509Cert.h"
#include "nsNSSIOLayer.h"
#include "nsThreadUtils.h"
#include "ssl.h"

class NSSSocketControl;

// NSS callback to select a client authentication certificate. See documentation
// at the top of TLSClientAuthCertSelection.cpp.
SECStatus SSLGetClientAuthDataHook(void* arg, PRFileDesc* socket,
                                   CERTDistNames* caNames,
                                   CERTCertificate** pRetCert,
                                   SECKEYPrivateKey** pRetKey);

// Base class for continuing the operation of selecting a client authentication
// certificate. Should not be used directly.
class ClientAuthCertificateSelectedBase : public mozilla::Runnable {
 public:
  ClientAuthCertificateSelectedBase()
      : Runnable("ClientAuthCertificateSelectedBase") {}

  // Call to indicate that a client authentication certificate has been
  // selected.
  void SetSelectedClientAuthData(
      nsTArray<uint8_t>&& selectedCertBytes,
      nsTArray<nsTArray<uint8_t>>&& selectedCertChainBytes);

 protected:
  nsTArray<uint8_t> mSelectedCertBytes;
  // The bytes of the certificates that form a chain from the selected
  // certificate to a root. Necessary so NSS can include them in the TLS
  // handshake (see note about mClientCertChain in NSSSocketControl).
  nsTArray<nsTArray<uint8_t>> mSelectedCertChainBytes;
};

class ClientAuthCertificateSelected : public ClientAuthCertificateSelectedBase {
 public:
  explicit ClientAuthCertificateSelected(NSSSocketControl* socketInfo)
      : mSocketInfo(socketInfo) {}

  NS_IMETHOD Run() override;

 private:
  RefPtr<NSSSocketControl> mSocketInfo;
};

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

  ClientAuthInfo(const ClientAuthInfo&) = delete;
  void operator=(const ClientAuthInfo&) = delete;

 private:
  nsCString mHostName;
  mozilla::OriginAttributes mOriginAttributes;
  int32_t mPort;
  uint32_t mProviderFlags;
  uint32_t mProviderTlsFlags;
};

// Helper runnable to select a client authentication certificate. Gets created
// on the socket thread or an IPC thread, runs on the main thread, and then runs
// its continuation on the socket thread.
class SelectClientAuthCertificate : public mozilla::Runnable {
 public:
  SelectClientAuthCertificate(
      ClientAuthInfo&& info, mozilla::UniqueCERTCertificate&& serverCert,
      nsTArray<nsTArray<uint8_t>>&& caNames,
      mozilla::UniqueCERTCertList&& potentialClientCertificates,
      ClientAuthCertificateSelectedBase* continuation, uint64_t browserId)
      : Runnable("SelectClientAuthCertificate"),
        mInfo(std::move(info)),
        mServerCert(std::move(serverCert)),
        mCANames(std::move(caNames)),
        mPotentialClientCertificates(std::move(potentialClientCertificates)),
        mContinuation(continuation),
        mBrowserId(browserId) {}

  NS_IMETHOD Run() override;

  const ClientAuthInfo& Info() { return mInfo; }
  void DispatchContinuation(nsTArray<uint8_t>&& selectedCertBytes);

 private:
  mozilla::pkix::Result BuildChainForCertificate(
      nsTArray<uint8_t>& certBytes,
      nsTArray<nsTArray<uint8_t>>& certChainBytes);

  ClientAuthInfo mInfo;
  mozilla::UniqueCERTCertificate mServerCert;
  nsTArray<nsTArray<uint8_t>> mCANames;
  mozilla::UniqueCERTCertList mPotentialClientCertificates;
  RefPtr<ClientAuthCertificateSelectedBase> mContinuation;

  nsTArray<nsTArray<uint8_t>> mEnterpriseCertificates;

  uint64_t mBrowserId;
  nsCOMPtr<nsIInterfaceRequestor> mSecurityCallbacks;
};

#endif  // SECURITY_MANAGER_SSL_TLSCLIENTAUTHCERTSELECTION_H_

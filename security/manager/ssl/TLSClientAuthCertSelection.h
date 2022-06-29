/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SECURITY_MANAGER_SSL_TLSCLIENTAUTHCERTSELECTION_H_
#define SECURITY_MANAGER_SSL_TLSCLIENTAUTHCERTSELECTION_H_

#include "nsIX509Cert.h"
#include "ssl.h"
#include "nsNSSIOLayer.h"

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
  // handshake (see note about mClientCertChain in nsNSSSocketInfo).
  nsTArray<nsTArray<uint8_t>> mSelectedCertChainBytes;
};

class ClientAuthCertificateSelected : public ClientAuthCertificateSelectedBase {
 public:
  explicit ClientAuthCertificateSelected(nsNSSSocketInfo* socketInfo)
      : mSocketInfo(socketInfo) {}

  NS_IMETHOD Run() override;

 private:
  RefPtr<nsNSSSocketInfo> mSocketInfo;
};

#endif  // SECURITY_MANAGER_SSL_TLSCLIENTAUTHCERTSELECTION_H_

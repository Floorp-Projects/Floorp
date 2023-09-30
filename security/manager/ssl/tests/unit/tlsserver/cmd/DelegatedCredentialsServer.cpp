/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a standalone server used to test Delegated Credentials
// (see: https://tools.ietf.org/html/draft-ietf-tls-subcerts-03).
//
// The client is expected to connect, initiate an SSL handshake (with SNI
// to indicate which "server" to connect to), and verify the certificate.
// If all is good, the client then sends one encrypted byte and receives that
// same byte back.
// This server also has the ability to "call back" another process waiting on
// it. That is, when the server is all set up and ready to receive connections,
// it will connect to a specified port and issue a simple HTTP request.

#include <iostream>

#include "TLSServer.h"

#include "sslexp.h"

using namespace mozilla;
using namespace mozilla::test;

struct DelegatedCertHost {
  const char* mHostName;
  const char* mCertName;
  const char* mDCKeyNick;
  bool mEnableDelegatedCredentials;
};

const PRUint32 kDCValidFor = 60 * 60 * 24 * 7 /* 1 week (seconds) */;

// {host, eeCert, dcCert, enableDC}
const DelegatedCertHost sDelegatedCertHosts[] = {
    {"delegated-enabled.example.com", "delegated-ee", "delegated.key", true},
    {"standard-enabled.example.com", "default-ee", "delegated.key", true},
    {"delegated-disabled.example.com", "delegated-ee",
     /* anything non-null */ "delegated.key", false},
    {nullptr, nullptr, nullptr, false}};

int32_t DoSNISocketConfig(PRFileDesc* aFd, const SECItem* aSrvNameArr,
                          uint32_t aSrvNameArrSize, void* aArg) {
  const DelegatedCertHost* host =
      GetHostForSNI(aSrvNameArr, aSrvNameArrSize, sDelegatedCertHosts);
  if (!host) {
    return SSL_SNI_SEND_ALERT;
  }

  if (gDebugLevel >= DEBUG_VERBOSE) {
    std::cerr << "Identified host " << host->mHostName << '\n';
  }

  UniqueCERTCertificate delegatorCert(
      PK11_FindCertFromNickname(host->mCertName, nullptr));
  if (!delegatorCert) {
    PrintPRError("PK11_FindCertFromNickname failed");
    return SSL_SNI_SEND_ALERT;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
    PrintPRError("PK11_GetInternalKeySlot failed");
    return SSL_SNI_SEND_ALERT;
  }

  SSLExtraServerCertData extra_data = {ssl_auth_null,
                                       /* Filled in by callee */ nullptr,
                                       nullptr,
                                       nullptr,
                                       /* DC */ nullptr,
                                       /* DC PrivKey */ nullptr};

  UniqueSECKEYPrivateKey delegatorPriv(
      PK11_FindKeyByDERCert(slot.get(), delegatorCert.get(), nullptr));
  if (!delegatorPriv) {
    PrintPRError("PK11_FindKeyByDERCert failed");
    return SSL_SNI_SEND_ALERT;
  }

  // Find the DC keypair by the file (nick) name.
  ScopedAutoSECItem dc;
  UniqueSECKEYPrivateKey dcPriv;
  if (host->mEnableDelegatedCredentials) {
    if (gDebugLevel >= DEBUG_VERBOSE) {
      std::cerr << "Enabling a delegated credential for host "
                << host->mHostName << '\n';
    }

    if (PK11_NeedLogin(slot.get())) {
      SECStatus rv = PK11_Authenticate(slot.get(), PR_TRUE, nullptr);
      if (rv != SECSuccess) {
        PrintPRError("PK11_Authenticate failed");
        return SSL_SNI_SEND_ALERT;
      }
    }
    UniqueSECKEYPrivateKeyList list(PK11_ListPrivKeysInSlot(
        slot.get(), const_cast<char*>(host->mDCKeyNick), nullptr));
    if (!list) {
      PrintPRError("PK11_ListPrivKeysInSlot failed");
      return SSL_SNI_SEND_ALERT;
    }
    SECKEYPrivateKeyListNode* node = PRIVKEY_LIST_HEAD(list);

    dcPriv.reset(SECKEY_CopyPrivateKey(node->key));
    if (!dcPriv) {
      PrintPRError("PK11_ListPrivKeysInSlot could not find dcPriv");
      return SSL_SNI_SEND_ALERT;
    }

    UniqueSECKEYPublicKey dcPub(SECKEY_ConvertToPublicKey(dcPriv.get()));
    if (!dcPub) {
      PrintPRError("SECKEY_ConvertToPublicKey failed");
      return SSL_SNI_SEND_ALERT;
    }

    // Create and set the DC.
    if (SSL_DelegateCredential(delegatorCert.get(), delegatorPriv.get(),
                               dcPub.get(), ssl_sig_ecdsa_secp384r1_sha384,
                               kDCValidFor, PR_Now(), &dc) != SECSuccess) {
      PrintPRError("SSL_DelegateCredential failed");
      return SSL_SNI_SEND_ALERT;
    }
    extra_data.delegCred = &dc;
    extra_data.delegCredPrivKey = dcPriv.get();

    // The list should only have a single key.
    PORT_Assert(PRIVKEY_LIST_END(PRIVKEY_LIST_NEXT(node), list));
  }

  if (ConfigSecureServerWithNamedCert(aFd, host->mCertName, nullptr, nullptr,
                                      &extra_data) != SECSuccess) {
    PrintPRError("ConfigSecureServerWithNamedCert failed");
    return SSL_SNI_SEND_ALERT;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  return StartServer(argc, argv, DoSNISocketConfig, nullptr);
}

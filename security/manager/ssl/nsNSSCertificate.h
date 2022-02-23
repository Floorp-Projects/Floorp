/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSCertificate_h
#define nsNSSCertificate_h

#include "ScopedNSSTypes.h"
#include "certt.h"
#include "mozilla/DataMutex.h"
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"
#include "nsIClassInfo.h"
#include "nsISerializable.h"
#include "nsIX509Cert.h"
#include "nsStringFwd.h"

class nsNSSCertificate final : public nsIX509Cert,
                               public nsISerializable,
                               public nsIClassInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERT
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  nsNSSCertificate();
  explicit nsNSSCertificate(CERTCertificate* cert);
  explicit nsNSSCertificate(nsTArray<uint8_t>&& der);

 private:
  virtual ~nsNSSCertificate() = default;
  nsresult GetCertificateHash(nsAString& aFingerprint, SECOidTag aHashAlg);
  mozilla::UniqueCERTCertificate GetOrInstantiateCert();

  nsTArray<uint8_t> mDER;
  // There may be multiple threads running when mCert is actually instantiated,
  // so it must be protected by a mutex.
  mozilla::DataMutex<mozilla::Maybe<mozilla::UniqueCERTCertificate>> mCert;
};

#define NS_X509CERT_CID                              \
  { /* 660a3226-915c-4ffb-bb20-8985a632df05 */       \
    0x660a3226, 0x915c, 0x4ffb, {                    \
      0xbb, 0x20, 0x89, 0x85, 0xa6, 0x32, 0xdf, 0x05 \
    }                                                \
  }

#endif  // nsNSSCertificate_h

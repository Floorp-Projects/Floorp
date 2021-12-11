/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSCertificate_h
#define nsNSSCertificate_h

#include <functional>
#include <vector>

#include "ScopedNSSTypes.h"
#include "certt.h"
#include "nsCOMPtr.h"
#include "nsIClassInfo.h"
#include "nsISerializable.h"
#include "nsIX509Cert.h"
#include "nsSimpleEnumerator.h"
#include "nsStringFwd.h"

namespace mozilla {
namespace pkix {
class DERArray;
}
}  // namespace mozilla

class nsINSSComponent;

class nsNSSCertificate final : public nsIX509Cert,
                               public nsISerializable,
                               public nsIClassInfo {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERT
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  explicit nsNSSCertificate(CERTCertificate* cert);
  nsNSSCertificate();
  static nsNSSCertificate* Create(CERTCertificate* cert = nullptr);
  static nsNSSCertificate* ConstructFromDER(char* certDER, int derLen);

  // This method assumes that the current list object
  // is ordered [end entity, intermediates..., root].
  // Will return error if used on self-signed or empty chains.
  // This method requires that the list `aIntermediates` must be empty.
  static nsresult GetIntermediatesAsDER(
      /* int */ const nsTArray<RefPtr<nsIX509Cert>>& aCertList,
      /* out */ nsTArray<nsTArray<uint8_t>>& aIntermediates);

  // Obtain the root certificate of a certificate chain. On an
  // empty list, leaves aRoot empty and returns a failure.
  // Assumes list is ordered [end entity, intermediates..., root].
  static nsresult GetRootCertificate(
      const nsTArray<RefPtr<nsIX509Cert>>& aCertList,
      /* out */ nsCOMPtr<nsIX509Cert>& aRoot);

 private:
  virtual ~nsNSSCertificate() = default;

  mozilla::UniqueCERTCertificate mCert;
  uint32_t mCertType;
  nsresult GetSortableDate(PRTime aTime, nsAString& _aSortableDate);
  bool InitFromDER(char* certDER, int derLen);  // return false on failure

  nsresult GetCertificateHash(nsAString& aFingerprint, SECOidTag aHashAlg);
};

namespace mozilla {

SECStatus ConstructCERTCertListFromReversedDERArray(
    const mozilla::pkix::DERArray& certArray,
    /*out*/ mozilla::UniqueCERTCertList& certList);

}  // namespace mozilla

#define NS_X509CERT_CID                              \
  { /* 660a3226-915c-4ffb-bb20-8985a632df05 */       \
    0x660a3226, 0x915c, 0x4ffb, {                    \
      0xbb, 0x20, 0x89, 0x85, 0xa6, 0x32, 0xdf, 0x05 \
    }                                                \
  }

#endif  // nsNSSCertificate_h

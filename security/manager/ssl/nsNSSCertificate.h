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
#include "nsIASN1Object.h"
#include "nsIClassInfo.h"
#include "nsISerializable.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertList.h"
#include "nsSimpleEnumerator.h"
#include "nsStringFwd.h"

namespace mozilla { namespace pkix { class DERArray; } }

class nsINSSComponent;
class nsIASN1Sequence;

class nsNSSCertificate final : public nsIX509Cert
                             , public nsISerializable
                             , public nsIClassInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERT
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  explicit nsNSSCertificate(CERTCertificate* cert);
  nsNSSCertificate();
  static nsNSSCertificate* Create(CERTCertificate* cert = nullptr);
  static nsNSSCertificate* ConstructFromDER(char* certDER, int derLen);

  // This is a separate static method so nsNSSComponent can use it during NSS
  // initialization. Other code should probably not use it.
  static nsresult GetDbKey(const mozilla::UniqueCERTCertificate& cert,
                           nsACString& aDbKey);

private:
  virtual ~nsNSSCertificate();

  mozilla::UniqueCERTCertificate mCert;
  bool             mPermDelete;
  uint32_t         mCertType;
  std::vector<nsString> mSubjectAltNames;
  nsresult CreateASN1Struct(nsIASN1Object** aRetVal);
  nsresult CreateTBSCertificateASN1Struct(nsIASN1Sequence** retSequence);
  nsresult GetSortableDate(PRTime aTime, nsAString& _aSortableDate);
  bool InitFromDER(char* certDER, int derLen);  // return false on failure

  nsresult GetCertificateHash(nsAString& aFingerprint, SECOidTag aHashAlg);
  void GetSubjectAltNames();
};

namespace mozilla {

SECStatus ConstructCERTCertListFromReversedDERArray(
            const mozilla::pkix::DERArray& certArray,
            /*out*/ mozilla::UniqueCERTCertList& certList);

} // namespace mozilla

typedef const std::function<nsresult(nsCOMPtr<nsIX509Cert>& aCert,
                bool aHasMore, /* out */ bool& aContinue)> ForEachCertOperation;

class nsNSSCertList : public nsIX509CertList
                    , public nsISerializable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTLIST
  NS_DECL_NSISERIALIZABLE

  // The only way to call this is with std::move(some cert list) (because the
  // copy constructor should be deleted for UniqueCERTCertList), so we
  // effectively take ownership of it. What actually happens is we iterate
  // through the list getting our own owned reference to each certificate in the
  // list, and then the UniqueCERTCertList is dropped as it goes out of scope
  // (thus releasing its own reference to each certificate).
  explicit nsNSSCertList(mozilla::UniqueCERTCertList certList);

  nsNSSCertList();

  static mozilla::UniqueCERTCertList DupCertList(
    const mozilla::UniqueCERTCertList& certList);

  // For each certificate in this CertList, run the operation aOperation.
  // To end early with NS_OK, set the `aContinue` argument false before
  // returning. To end early with an error, return anything except NS_OK.
  // The `aHasMore` argument is false when this is the last certificate in the
  // chain.
  nsresult ForEachCertificateInChain(ForEachCertOperation& aOperation);

  // Split a certificate chain into the root, intermediates (if any), and end
  // entity. This method does so blindly, assuming that the current list object
  // is ordered [end entity, intermediates..., root]. If that isn't true, this
  // method will return the certificates at the two ends without regard to the
  // actual chain of trust. Callers are encouraged to check, if there's any
  // doubt.
  // Will return error if used on self-signed or empty chains.
  // This method requires that all arguments be empty, notably the list
  // `aIntermediates` must be empty.
  nsresult SegmentCertificateChain(/* out */ nsCOMPtr<nsIX509Cert>& aRoot,
                           /* out */ nsCOMPtr<nsIX509CertList>& aIntermediates,
                           /* out */ nsCOMPtr<nsIX509Cert>& aEndEntity);

  // Obtain the root certificate of a certificate chain. This method does so
  // blindly, as SegmentCertificateChain; the same restrictions apply. On an
  // empty list, leaves aRoot empty and returns OK.
  nsresult GetRootCertificate(/* out */ nsCOMPtr<nsIX509Cert>& aRoot);

private:
   virtual ~nsNSSCertList() {}

   std::vector<mozilla::UniqueCERTCertificate> mCerts;

   nsNSSCertList(const nsNSSCertList&) = delete;
   void operator=(const nsNSSCertList&) = delete;
};

#define NS_X509CERT_CID { /* 660a3226-915c-4ffb-bb20-8985a632df05 */   \
    0x660a3226,                                                        \
    0x915c,                                                            \
    0x4ffb,                                                            \
    { 0xbb, 0x20, 0x89, 0x85, 0xa6, 0x32, 0xdf, 0x05 }                 \
  }

#endif // nsNSSCertificate_h

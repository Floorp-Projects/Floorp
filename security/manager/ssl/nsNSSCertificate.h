/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSCertificate_h
#define nsNSSCertificate_h

#include "ScopedNSSTypes.h"
#include "certt.h"
#include "nsCOMPtr.h"
#include "nsIASN1Object.h"
#include "nsIClassInfo.h"
#include "nsISerializable.h"
#include "nsISimpleEnumerator.h"
#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertList.h"
#include "nsNSSShutDown.h"

namespace mozilla { namespace pkix { class DERArray; } }

class nsAutoString;
class nsINSSComponent;
class nsIASN1Sequence;

class nsNSSCertificate final : public nsIX509Cert,
                               public nsISerializable,
                               public nsIClassInfo,
                               public nsNSSShutDownObject
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERT
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  friend class nsNSSCertificateFakeTransport;

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
  nsresult CreateASN1Struct(nsIASN1Object** aRetVal);
  nsresult CreateTBSCertificateASN1Struct(nsIASN1Sequence** retSequence,
                                          nsINSSComponent* nssComponent);
  nsresult GetSortableDate(PRTime aTime, nsAString& _aSortableDate);
  virtual void virtualDestroyNSSReference() override;
  void destructorSafeDestroyNSSReference();
  bool InitFromDER(char* certDER, int derLen);  // return false on failure

  nsresult GetCertificateHash(nsAString& aFingerprint, SECOidTag aHashAlg);
};

namespace mozilla {

SECStatus ConstructCERTCertListFromReversedDERArray(
            const mozilla::pkix::DERArray& certArray,
            /*out*/ mozilla::UniqueCERTCertList& certList);

} // namespace mozilla

class nsNSSCertList: public nsIX509CertList,
                     public nsISerializable,
                     public nsNSSShutDownObject
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTLIST
  NS_DECL_NSISERIALIZABLE

  // certList is adopted
  nsNSSCertList(mozilla::UniqueCERTCertList certList,
                const nsNSSShutDownPreventionLock& proofOfLock);

  nsNSSCertList();

  static mozilla::UniqueCERTCertList DupCertList(
    const mozilla::UniqueCERTCertList& certList,
    const nsNSSShutDownPreventionLock& proofOfLock);

private:
   virtual ~nsNSSCertList();
   virtual void virtualDestroyNSSReference() override;
   void destructorSafeDestroyNSSReference();

   mozilla::UniqueCERTCertList mCertList;

   nsNSSCertList(const nsNSSCertList&) = delete;
   void operator=(const nsNSSCertList&) = delete;
};

class nsNSSCertListEnumerator: public nsISimpleEnumerator,
                               public nsNSSShutDownObject
{
public:
   NS_DECL_THREADSAFE_ISUPPORTS
   NS_DECL_NSISIMPLEENUMERATOR

   nsNSSCertListEnumerator(const mozilla::UniqueCERTCertList& certList,
                           const nsNSSShutDownPreventionLock& proofOfLock);
private:
   virtual ~nsNSSCertListEnumerator();
   virtual void virtualDestroyNSSReference() override;
   void destructorSafeDestroyNSSReference();

   mozilla::UniqueCERTCertList mCertList;

   nsNSSCertListEnumerator(const nsNSSCertListEnumerator&) = delete;
   void operator=(const nsNSSCertListEnumerator&) = delete;
};

#define NS_X509CERT_CID { /* 660a3226-915c-4ffb-bb20-8985a632df05 */   \
    0x660a3226,                                                        \
    0x915c,                                                            \
    0x4ffb,                                                            \
    { 0xbb, 0x20, 0x89, 0x85, 0xa6, 0x32, 0xdf, 0x05 }                 \
  }

#endif // nsNSSCertificate_h

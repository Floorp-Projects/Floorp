/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_NSSCERTIFICATE_H_
#define _NS_NSSCERTIFICATE_H_

#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertList.h"
#include "nsIASN1Object.h"
#include "nsCOMPtr.h"
#include "nsNSSShutDown.h"
#include "nsISimpleEnumerator.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "ScopedNSSTypes.h"
#include "certt.h"

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

  explicit nsNSSCertificate(CERTCertificate* cert, SECOidTag* evOidPolicy = nullptr);
  nsNSSCertificate();
  nsresult FormatUIStrings(const nsAutoString& nickname,
                           nsAutoString& nickWithSerial,
                           nsAutoString& details);
  static nsNSSCertificate* Create(CERTCertificate*cert = nullptr,
                                  SECOidTag* evOidPolicy = nullptr);
  static nsNSSCertificate* ConstructFromDER(char* certDER, int derLen);
  nsresult GetIsExtendedValidation(bool* aIsEV);

  enum EVStatus {
    ev_status_invalid = 0,
    ev_status_valid = 1,
    ev_status_unknown = 2
  };

private:
  virtual ~nsNSSCertificate();

  mozilla::ScopedCERTCertificate mCert;
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

  EVStatus mCachedEVStatus;
  SECOidTag mCachedEVOidTag;
  nsresult hasValidEVOidTag(SECOidTag& resultOidTag, bool& validEV);
  nsresult getValidEVOidTag(SECOidTag& resultOidTag, bool& validEV);
};

namespace mozilla {

SECStatus ConstructCERTCertListFromReversedDERArray(
            const mozilla::pkix::DERArray& certArray,
            /*out*/ mozilla::ScopedCERTCertList& certList);

} // namespcae mozilla

class nsNSSCertList: public nsIX509CertList,
                     public nsISerializable,
                     public nsNSSShutDownObject
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTLIST
  NS_DECL_NSISERIALIZABLE

  // certList is adopted
  nsNSSCertList(mozilla::ScopedCERTCertList& certList,
                const nsNSSShutDownPreventionLock& proofOfLock);

  nsNSSCertList();

  static CERTCertList* DupCertList(CERTCertList* aCertList,
                                   const nsNSSShutDownPreventionLock&
                                     proofOfLock);
private:
   virtual ~nsNSSCertList();
   virtual void virtualDestroyNSSReference() override;
   void destructorSafeDestroyNSSReference();

   mozilla::ScopedCERTCertList mCertList;

   nsNSSCertList(const nsNSSCertList&) = delete;
   void operator=(const nsNSSCertList&) = delete;
};

class nsNSSCertListEnumerator: public nsISimpleEnumerator,
                               public nsNSSShutDownObject
{
public:
   NS_DECL_THREADSAFE_ISUPPORTS
   NS_DECL_NSISIMPLEENUMERATOR

   nsNSSCertListEnumerator(CERTCertList* certList,
                           const nsNSSShutDownPreventionLock& proofOfLock);
private:
   virtual ~nsNSSCertListEnumerator();
   virtual void virtualDestroyNSSReference() override;
   void destructorSafeDestroyNSSReference();

   mozilla::ScopedCERTCertList mCertList;

   nsNSSCertListEnumerator(const nsNSSCertListEnumerator&) = delete;
   void operator=(const nsNSSCertListEnumerator&) = delete;
};


#define NS_NSS_LONG 4
#define NS_NSS_GET_LONG(x) ((((unsigned long)((x)[0])) << 24) | \
                            (((unsigned long)((x)[1])) << 16) | \
                            (((unsigned long)((x)[2])) <<  8) | \
                             ((unsigned long)((x)[3])) )
#define NS_NSS_PUT_LONG(src,dest) (dest)[0] = (((src) >> 24) & 0xff); \
                                  (dest)[1] = (((src) >> 16) & 0xff); \
                                  (dest)[2] = (((src) >>  8) & 0xff); \
                                  (dest)[3] = ((src) & 0xff);

#define NS_X509CERT_CID { /* 660a3226-915c-4ffb-bb20-8985a632df05 */   \
    0x660a3226,                                                        \
    0x915c,                                                            \
    0x4ffb,                                                            \
    { 0xbb, 0x20, 0x89, 0x85, 0xa6, 0x32, 0xdf, 0x05 }                 \
  }

#endif // _NS_NSSCERTIFICATE_H_

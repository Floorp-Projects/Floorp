/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NS_NSSCERTIFICATE_H_
#define _NS_NSSCERTIFICATE_H_

#include "nsIX509Cert.h"
#include "nsIX509Cert2.h"
#include "nsIX509Cert3.h"
#include "nsIX509CertDB.h"
#include "nsIX509CertList.h"
#include "nsIASN1Object.h"
#include "nsISMimeCert.h"
#include "nsIIdentityInfo.h"
#include "nsCOMPtr.h"
#include "nsNSSShutDown.h"
#include "nsISimpleEnumerator.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "insanity/pkixtypes.h"
#include "certt.h"

class nsAutoString;
class nsINSSComponent;
class nsIASN1Sequence;

class nsNSSCertificate : public nsIX509Cert3,
                         public nsIIdentityInfo,
                         public nsISMimeCert,
                         public nsISerializable,
                         public nsIClassInfo,
                         public nsNSSShutDownObject
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERT
  NS_DECL_NSIX509CERT2
  NS_DECL_NSIX509CERT3
  NS_DECL_NSIIDENTITYINFO
  NS_DECL_NSISMIMECERT
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  nsNSSCertificate(CERTCertificate* cert, SECOidTag* evOidPolicy = nullptr);
  nsNSSCertificate();
  virtual ~nsNSSCertificate();
  nsresult FormatUIStrings(const nsAutoString& nickname,
                           nsAutoString& nickWithSerial,
                           nsAutoString& details);
  static nsNSSCertificate* Create(CERTCertificate*cert = nullptr,
                                  SECOidTag* evOidPolicy = nullptr);
  static nsNSSCertificate* ConstructFromDER(char* certDER, int derLen);

private:
  insanity::pkix::ScopedCERTCertificate mCert;
  bool             mPermDelete;
  uint32_t         mCertType;
  nsresult CreateASN1Struct(nsIASN1Object** aRetVal);
  nsresult CreateTBSCertificateASN1Struct(nsIASN1Sequence** retSequence,
                                          nsINSSComponent* nssComponent);
  nsresult GetSortableDate(PRTime aTime, nsAString& _aSortableDate);
  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
  bool InitFromDER(char* certDER, int derLen);  // return false on failure

  enum {
    ev_status_unknown = -1, ev_status_invalid = 0, ev_status_valid = 1
  } mCachedEVStatus;
  SECOidTag mCachedEVOidTag;
  nsresult hasValidEVOidTag(SECOidTag& resultOidTag, bool& validEV);
  nsresult getValidEVOidTag(SECOidTag& resultOidTag, bool& validEV);
};

class nsNSSCertList: public nsIX509CertList,
                     public nsNSSShutDownObject
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTLIST

  // certList is adopted
  nsNSSCertList(insanity::pkix::ScopedCERTCertList& certList,
                const nsNSSShutDownPreventionLock& proofOfLock);

  nsNSSCertList();

  static CERTCertList* DupCertList(CERTCertList* aCertList,
                                   const nsNSSShutDownPreventionLock&
                                     proofOfLock);
private:
   virtual ~nsNSSCertList();
   virtual void virtualDestroyNSSReference();
   void destructorSafeDestroyNSSReference();

   insanity::pkix::ScopedCERTCertList mCertList;

   nsNSSCertList(const nsNSSCertList&) MOZ_DELETE;
   void operator=(const nsNSSCertList&) MOZ_DELETE;
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
   virtual void virtualDestroyNSSReference();
   void destructorSafeDestroyNSSReference();

   insanity::pkix::ScopedCERTCertList mCertList;

   nsNSSCertListEnumerator(const nsNSSCertListEnumerator&) MOZ_DELETE;
   void operator=(const nsNSSCertListEnumerator&) MOZ_DELETE;
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

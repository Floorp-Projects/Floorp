/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSNSSCERTIFICATEDB_H__
#define __NSNSSCERTIFICATEDB_H__

#include "nsIX509CertDB.h"
#include "nsIX509CertDB2.h"
#include "nsNSSShutDown.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Mutex.h"
#include "certt.h"

class nsCString;
class nsIArray;
class nsRecentBadCerts;

class nsNSSCertificateDB : public nsIX509CertDB
                         , public nsIX509CertDB2
                         , public nsNSSShutDownObject

{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTDB
  NS_DECL_NSIX509CERTDB2

  nsNSSCertificateDB(); 

  // Use this function to generate a default nickname for a user
  // certificate that is to be imported onto a token.
  static void
  get_default_nickname(CERTCertificate *cert, nsIInterfaceRequestor* ctx,
                       nsCString &nickname,
                       const nsNSSShutDownPreventionLock &proofOfLock);

  static nsresult 
  ImportValidCACerts(int numCACerts, SECItem *CACerts, nsIInterfaceRequestor *ctx,
                     const nsNSSShutDownPreventionLock &proofOfLock);

protected:
  virtual ~nsNSSCertificateDB();

private:

  static nsresult
  ImportValidCACertsInList(CERTCertList *certList, nsIInterfaceRequestor *ctx,
                           const nsNSSShutDownPreventionLock &proofOfLock);

  static void DisplayCertificateAlert(nsIInterfaceRequestor *ctx, 
                                      const char *stringID, nsIX509Cert *certToShow,
                                      const nsNSSShutDownPreventionLock &proofOfLock);

  void getCertNames(CERTCertList *certList,
                    uint32_t      type, 
                    uint32_t     *_count,
                    char16_t  ***_certNameList,
                    const nsNSSShutDownPreventionLock &proofOfLock);

  CERTDERCerts *getCertsFromPackage(PLArenaPool *arena, uint8_t *data, 
                                    uint32_t length,
                                    const nsNSSShutDownPreventionLock &proofOfLock);
  nsresult handleCACertDownload(nsIArray *x509Certs, 
                                nsIInterfaceRequestor *ctx,
                                const nsNSSShutDownPreventionLock &proofOfLock);

  mozilla::Mutex mBadCertsLock;
  mozilla::RefPtr<nsRecentBadCerts> mPublicRecentBadCerts;
  mozilla::RefPtr<nsRecentBadCerts> mPrivateRecentBadCerts;

  // We don't own any NSS objects here, so no need to clean up
  virtual void virtualDestroyNSSReference() { };
};

#define NS_X509CERTDB_CID { /* fb0bbc5c-452e-4783-b32c-80124693d871 */ \
    0xfb0bbc5c,                                                        \
    0x452e,                                                            \
    0x4783,                                                            \
    {0xb3, 0x2c, 0x80, 0x12, 0x46, 0x93, 0xd8, 0x71}                   \
  }

#endif

/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Ian McGreer <mcgreer@netscape.com>
 *  Javier Delgadillo <javi@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */

#include "prmem.h"
#include "prerror.h"
#include "prprf.h"

#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsILocalFile.h"
#include "nsNSSCertificate.h"
#include "nsPKCS12Blob.h"
#include "nsPK11TokenDB.h"
#include "nsIX509Cert.h"
#include "nsINSSDialogs.h"
#include "nsNSSASN1Object.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsILocaleService.h"
#include "nsIURI.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsTime.h"
#include "nsIProxyObjectManager.h"
#include "nsCRT.h"
#include "nsAutoLock.h"

#include "nspr.h"
extern "C" {
#include "pk11func.h"
#include "certdb.h"
#include "cert.h"
#include "secerr.h"
#include "nssb64.h"
#include "secasn1.h"
#include "secder.h"
}
#include "ssl.h"
#include "ocsp.h"
#include "plbase64.h"

#include "nsNSSCleaner.h"
NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)
NSSCleanupAutoPtrClass(CERTCertList, CERT_DestroyCertList)

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);

/*
 * nsNSSCertTrust
 * 
 * Class for maintaining trust flags for an NSS certificate.
 */
class nsNSSCertTrust
{
public:
  nsNSSCertTrust();
  nsNSSCertTrust(unsigned int ssl, unsigned int email, unsigned int objsign);
  nsNSSCertTrust(CERTCertTrust *t);
  virtual ~nsNSSCertTrust();

  /* query */
  PRBool HasAnyCA();
  PRBool HasAnyUser();
  PRBool HasCA(PRBool checkSSL = PR_TRUE, 
               PRBool checkEmail = PR_TRUE,  
               PRBool checkObjSign = PR_TRUE);
  PRBool HasPeer(PRBool checkSSL = PR_TRUE, 
                 PRBool checkEmail = PR_TRUE,  
                 PRBool checkObjSign = PR_TRUE);
  PRBool HasUser(PRBool checkSSL = PR_TRUE, 
                 PRBool checkEmail = PR_TRUE,  
                 PRBool checkObjSign = PR_TRUE);
  PRBool HasTrustedCA(PRBool checkSSL = PR_TRUE, 
                      PRBool checkEmail = PR_TRUE,  
                      PRBool checkObjSign = PR_TRUE);
  PRBool HasTrustedPeer(PRBool checkSSL = PR_TRUE, 
                        PRBool checkEmail = PR_TRUE,  
                        PRBool checkObjSign = PR_TRUE);

  /* common defaults */
  /* equivalent to "c,c,c" */
  void SetValidCA();
  /* equivalent to "C,C,C" */
  void SetTrustedServerCA();
  /* equivalent to "CT,CT,CT" */
  void SetTrustedCA();
  /* equivalent to "p,," */
  void SetValidServerPeer();
  /* equivalent to "p,p,p" */
  void SetValidPeer();
  /* equivalent to "P,P,P" */
  void SetTrustedPeer();
  /* equivalent to "u,u,u" */
  void SetUser();

  /* general setters */
  /* read: "p, P, c, C, T, u, w" */
  void SetSSLTrust(PRBool peer, PRBool tPeer,
                   PRBool ca,   PRBool tCA, PRBool tClientCA,
                   PRBool user, PRBool warn); 

  void SetEmailTrust(PRBool peer, PRBool tPeer,
                     PRBool ca,   PRBool tCA, PRBool tClientCA,
                     PRBool user, PRBool warn);

  void SetObjSignTrust(PRBool peer, PRBool tPeer,
                       PRBool ca,   PRBool tCA, PRBool tClientCA,
                       PRBool user, PRBool warn);

  /* set c <--> CT */
  void AddCATrust(PRBool ssl, PRBool email, PRBool objSign);
  /* set p <--> P */
  void AddPeerTrust(PRBool ssl, PRBool email, PRBool objSign);

  /* get it (const?) (shallow?) */
  CERTCertTrust * GetTrust() { return &mTrust; }

private:
  void addTrust(unsigned int *t, unsigned int v);
  void removeTrust(unsigned int *t, unsigned int v);
  PRBool hasTrust(unsigned int t, unsigned int v);
  CERTCertTrust mTrust;
};

void
nsNSSCertTrust::AddCATrust(PRBool ssl, PRBool email, PRBool objSign)
{
  if (ssl) {
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CA);
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
  if (email) {
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CA);
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
  if (objSign) {
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED_CA);
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
}

void
nsNSSCertTrust::AddPeerTrust(PRBool ssl, PRBool email, PRBool objSign)
{
  if (ssl)
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED);
  if (email)
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED);
  if (objSign)
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED);
}

nsNSSCertTrust::nsNSSCertTrust()
{
  memset(&mTrust, 0, sizeof(CERTCertTrust));
}

nsNSSCertTrust::nsNSSCertTrust(unsigned int ssl, 
                               unsigned int email, 
                               unsigned int objsign)
{
  memset(&mTrust, 0, sizeof(CERTCertTrust));
  addTrust(&mTrust.sslFlags, ssl);
  addTrust(&mTrust.emailFlags, email);
  addTrust(&mTrust.objectSigningFlags, objsign);
}

nsNSSCertTrust::nsNSSCertTrust(CERTCertTrust *t)
{
  if (t)
    memcpy(&mTrust, t, sizeof(CERTCertTrust));
  else
    memset(&mTrust, 0, sizeof(CERTCertTrust)); 
}

nsNSSCertTrust::~nsNSSCertTrust()
{
}

void
nsNSSCertTrust::SetSSLTrust(PRBool peer, PRBool tPeer,
                            PRBool ca,   PRBool tCA, PRBool tClientCA,
                            PRBool user, PRBool warn)
{
  mTrust.sslFlags = 0;
  if (peer || tPeer)
    addTrust(&mTrust.sslFlags, CERTDB_VALID_PEER);
  if (tPeer)
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED);
  if (ca || tCA)
    addTrust(&mTrust.sslFlags, CERTDB_VALID_CA);
  if (tClientCA)
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA)
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CA);
  if (user)
    addTrust(&mTrust.sslFlags, CERTDB_USER);
  if (warn)
    addTrust(&mTrust.sslFlags, CERTDB_SEND_WARN);
}

void
nsNSSCertTrust::SetEmailTrust(PRBool peer, PRBool tPeer,
                              PRBool ca,   PRBool tCA, PRBool tClientCA,
                              PRBool user, PRBool warn)
{
  mTrust.emailFlags = 0;
  if (peer || tPeer)
    addTrust(&mTrust.emailFlags, CERTDB_VALID_PEER);
  if (tPeer)
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED);
  if (ca || tCA)
    addTrust(&mTrust.emailFlags, CERTDB_VALID_CA);
  if (tClientCA)
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA)
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CA);
  if (user)
    addTrust(&mTrust.emailFlags, CERTDB_USER);
  if (warn)
    addTrust(&mTrust.emailFlags, CERTDB_SEND_WARN);
}

void
nsNSSCertTrust::SetObjSignTrust(PRBool peer, PRBool tPeer,
                                PRBool ca,   PRBool tCA, PRBool tClientCA,
                                PRBool user, PRBool warn)
{
  mTrust.objectSigningFlags = 0;
  if (peer || tPeer)
    addTrust(&mTrust.objectSigningFlags, CERTDB_VALID_PEER);
  if (tPeer)
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED);
  if (ca || tCA)
    addTrust(&mTrust.objectSigningFlags, CERTDB_VALID_CA);
  if (tClientCA)
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA)
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED_CA);
  if (user)
    addTrust(&mTrust.objectSigningFlags, CERTDB_USER);
  if (warn)
    addTrust(&mTrust.objectSigningFlags, CERTDB_SEND_WARN);
}

void
nsNSSCertTrust::SetValidCA()
{
  SetSSLTrust(PR_FALSE, PR_FALSE,
              PR_TRUE, PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_TRUE, PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_TRUE, PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void
nsNSSCertTrust::SetTrustedServerCA()
{
  SetSSLTrust(PR_FALSE, PR_FALSE,
              PR_TRUE, PR_TRUE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_TRUE, PR_TRUE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_TRUE, PR_TRUE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void
nsNSSCertTrust::SetTrustedCA()
{
  SetSSLTrust(PR_FALSE, PR_FALSE,
              PR_TRUE, PR_TRUE, PR_TRUE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_TRUE, PR_TRUE, PR_TRUE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_TRUE, PR_TRUE, PR_TRUE,
                  PR_FALSE, PR_FALSE);
}

void 
nsNSSCertTrust::SetValidPeer()
{
  SetSSLTrust(PR_TRUE, PR_FALSE,
              PR_FALSE, PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_TRUE, PR_FALSE,
                PR_FALSE, PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_TRUE, PR_FALSE,
                  PR_FALSE, PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void 
nsNSSCertTrust::SetValidServerPeer()
{
  SetSSLTrust(PR_TRUE, PR_FALSE,
              PR_FALSE, PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void 
nsNSSCertTrust::SetTrustedPeer()
{
  SetSSLTrust(PR_TRUE, PR_TRUE,
              PR_FALSE, PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_TRUE, PR_TRUE,
                PR_FALSE, PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_TRUE, PR_TRUE,
                  PR_FALSE, PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void
nsNSSCertTrust::SetUser()
{
  SetSSLTrust(PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE, PR_FALSE,
              PR_TRUE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE, PR_FALSE,
                PR_TRUE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE, PR_FALSE,
                  PR_TRUE, PR_FALSE);
}

PRBool
nsNSSCertTrust::HasAnyCA()
{
  if (hasTrust(mTrust.sslFlags, CERTDB_VALID_CA) ||
      hasTrust(mTrust.emailFlags, CERTDB_VALID_CA) ||
      hasTrust(mTrust.objectSigningFlags, CERTDB_VALID_CA))
    return PR_TRUE;
  return PR_FALSE;
}

PRBool
nsNSSCertTrust::HasCA(PRBool checkSSL, 
                      PRBool checkEmail,  
                      PRBool checkObjSign)
{
  if (checkSSL && !hasTrust(mTrust.sslFlags, CERTDB_VALID_CA))
    return PR_FALSE;
  if (checkEmail && !hasTrust(mTrust.emailFlags, CERTDB_VALID_CA))
    return PR_FALSE;
  if (checkObjSign && !hasTrust(mTrust.objectSigningFlags, CERTDB_VALID_CA))
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
nsNSSCertTrust::HasPeer(PRBool checkSSL, 
                        PRBool checkEmail,  
                        PRBool checkObjSign)
{
  if (checkSSL && !hasTrust(mTrust.sslFlags, CERTDB_VALID_PEER))
    return PR_FALSE;
  if (checkEmail && !hasTrust(mTrust.emailFlags, CERTDB_VALID_PEER))
    return PR_FALSE;
  if (checkObjSign && !hasTrust(mTrust.objectSigningFlags, CERTDB_VALID_PEER))
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
nsNSSCertTrust::HasAnyUser()
{
  if (hasTrust(mTrust.sslFlags, CERTDB_USER) ||
      hasTrust(mTrust.emailFlags, CERTDB_USER) ||
      hasTrust(mTrust.objectSigningFlags, CERTDB_USER))
    return PR_TRUE;
  return PR_FALSE;
}

PRBool
nsNSSCertTrust::HasUser(PRBool checkSSL, 
                        PRBool checkEmail,  
                        PRBool checkObjSign)
{
  if (checkSSL && !hasTrust(mTrust.sslFlags, CERTDB_USER))
    return PR_FALSE;
  if (checkEmail && !hasTrust(mTrust.emailFlags, CERTDB_USER))
    return PR_FALSE;
  if (checkObjSign && !hasTrust(mTrust.objectSigningFlags, CERTDB_USER))
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
nsNSSCertTrust::HasTrustedCA(PRBool checkSSL, 
                             PRBool checkEmail,  
                             PRBool checkObjSign)
{
  if (checkSSL && !(hasTrust(mTrust.sslFlags, CERTDB_TRUSTED_CA) ||
                    hasTrust(mTrust.sslFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return PR_FALSE;
  if (checkEmail && !(hasTrust(mTrust.emailFlags, CERTDB_TRUSTED_CA) ||
                      hasTrust(mTrust.emailFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return PR_FALSE;
  if (checkObjSign && 
       !(hasTrust(mTrust.objectSigningFlags, CERTDB_TRUSTED_CA) ||
         hasTrust(mTrust.objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return PR_FALSE;
  return PR_TRUE;
}

PRBool
nsNSSCertTrust::HasTrustedPeer(PRBool checkSSL, 
                               PRBool checkEmail,  
                               PRBool checkObjSign)
{
  if (checkSSL && !(hasTrust(mTrust.sslFlags, CERTDB_TRUSTED)))
    return PR_FALSE;
  if (checkEmail && !(hasTrust(mTrust.emailFlags, CERTDB_TRUSTED)))
    return PR_FALSE;
  if (checkObjSign && 
       !(hasTrust(mTrust.objectSigningFlags, CERTDB_TRUSTED)))
    return PR_FALSE;
  return PR_TRUE;
}

void
nsNSSCertTrust::addTrust(unsigned int *t, unsigned int v)
{
  *t |= v;
}

PRBool
nsNSSCertTrust::hasTrust(unsigned int t, unsigned int v)
{
  return (t & v);
}

/* Header file */
class nsX509CertValidity : public nsIX509CertValidity
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIX509CERTVALIDITY

  nsX509CertValidity();
  nsX509CertValidity(CERTCertificate *cert);
  virtual ~nsX509CertValidity();
  /* additional members */

private:
  PRTime mNotBefore, mNotAfter;
  PRBool mTimesInitialized;
};

/* Implementation file */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsX509CertValidity, nsIX509CertValidity)

nsX509CertValidity::nsX509CertValidity() : mTimesInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsX509CertValidity::nsX509CertValidity(CERTCertificate *cert) : 
                                           mTimesInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  if (cert) {
    SECStatus rv = CERT_GetCertTimes(cert, &mNotBefore, &mNotAfter);
    if (rv == SECSuccess)
      mTimesInitialized = PR_TRUE;
  }
}

nsX509CertValidity::~nsX509CertValidity()
{
  /* destructor code */
}

/* readonly attribute PRTime notBefore; */
NS_IMETHODIMP nsX509CertValidity::GetNotBefore(PRTime *aNotBefore)
{
  NS_ENSURE_ARG(aNotBefore);

  nsresult rv = NS_ERROR_FAILURE;
  if (mTimesInitialized) {
    *aNotBefore = mNotBefore;
    rv = NS_OK;
  }
  return rv;
}

/* readonly attribute PRTime notBeforeLocalTime; */
NS_IMETHODIMP nsX509CertValidity::GetNotBeforeLocalTime(PRUnichar **aNotBeforeLocalTime)
{
  NS_ENSURE_ARG(aNotBeforeLocalTime);
  
  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotBefore, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                                      &explodedTime, date);
  *aNotBeforeLocalTime = ToNewUnicode(date);
  return NS_OK;
}

/* readonly attribute PRTime notBeforeGMT; */
NS_IMETHODIMP nsX509CertValidity::GetNotBeforeGMT(PRUnichar **aNotBeforeGMT)
{
  NS_ENSURE_ARG(aNotBeforeGMT);

  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotBefore, PR_GMTParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                                      &explodedTime, date);
  *aNotBeforeGMT = ToNewUnicode(date);
  return NS_OK;
}

/* readonly attribute PRTime notAfter; */
NS_IMETHODIMP nsX509CertValidity::GetNotAfter(PRTime *aNotAfter)
{
  NS_ENSURE_ARG(aNotAfter);

  nsresult rv = NS_ERROR_FAILURE;
  if (mTimesInitialized) {
    *aNotAfter = mNotAfter;
    rv = NS_OK;
  }
  return rv;
}

/* readonly attribute PRTime notAfterLocalTime; */
NS_IMETHODIMP nsX509CertValidity::GetNotAfterLocalTime(PRUnichar **aNotAfterLocaltime)
{
  NS_ENSURE_ARG(aNotAfterLocaltime);

  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotAfter, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                                      &explodedTime, date);
  *aNotAfterLocaltime = ToNewUnicode(date);
  return NS_OK;
}

/* readonly attribute PRTime notAfterGMT; */
NS_IMETHODIMP nsX509CertValidity::GetNotAfterGMT(PRUnichar **aNotAfterGMT)
{
  NS_ENSURE_ARG(aNotAfterGMT);

  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotAfter, PR_GMTParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                                      &explodedTime, date);
  *aNotAfterGMT = ToNewUnicode(date);
  return NS_OK;
}

/* nsNSSCertificate */

NS_IMPL_THREADSAFE_ISUPPORTS1(nsNSSCertificate, nsIX509Cert)

nsNSSCertificate*
nsNSSCertificate::ConstructFromDER(char *certDER, int derLen)
{
  if (!certDER || !derLen)
    return nsnull;

  CERTCertificate *aCert = CERT_DecodeCertFromPackage(certDER, derLen);
  
  if (!aCert)
    return nsnull;

  if(aCert->dbhandle == nsnull)
  {
    aCert->dbhandle = CERT_GetDefaultCertDB();
  }

  // We don't want the new NSS cert to be dupped, therefore we create our instance
  // with a NULL pointer first, and set the contained cert later.

  nsNSSCertificate *newObject = new nsNSSCertificate(nsnull);
  
  if (!newObject)
  {
    CERT_DestroyCertificate(aCert);
    return nsnull;
  }
  
  newObject->mCert = aCert;
  return newObject;
}

nsNSSCertificate::nsNSSCertificate(CERTCertificate *cert) : 
                                           mPermDelete(PR_FALSE),
                                           mCertType(nsIX509Cert::UNKNOWN_CERT)
{
  NS_INIT_ISUPPORTS();

  if (cert) 
    mCert = CERT_DupCertificate(cert);
  else
    mCert = nsnull;
}

nsNSSCertificate::~nsNSSCertificate()
{
  if (mPermDelete) {
    if (mCertType == nsNSSCertificate::USER_CERT) {
      nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();
      PK11_DeleteTokenCertAndKey(mCert, cxt);
      CERT_DestroyCertificate(mCert);
    } else if (!PK11_IsReadOnly(mCert->slot)) {
      // If the cert isn't a user cert and it is on an external token, 
      // then we'll just leave it as untrusted, but won't delete it 
      // from the cert db.
      SEC_DeletePermCertificate(mCert);
    }
  } else {
    if (mCert)
      CERT_DestroyCertificate(mCert);
  }
}

nsresult
nsNSSCertificate::SetCertType(PRUint32 aCertType)
{
  mCertType = aCertType;
  return NS_OK;
}

nsresult
nsNSSCertificate::GetCertType(PRUint32 *aCertType)
{
  *aCertType = mCertType;
  return NS_OK;
}

nsresult
nsNSSCertificate::MarkForPermDeletion()
{
  // make sure user is logged in to the token
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();

  if (PK11_NeedLogin(mCert->slot)
      && !PK11_NeedUserInit(mCert->slot)
      && !PK11_IsInternal(mCert->slot))
  {
    if (SECSuccess != PK11_Authenticate(mCert->slot, PR_TRUE, ctx))
    {
      return NS_ERROR_FAILURE;
    }
  }

  mPermDelete = PR_TRUE;
  return NS_OK;
}

nsresult
nsNSSCertificate::FormatUIStrings(const nsAutoString &nickname, nsAutoString &nickWithSerial, nsAutoString &details)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIProxyObjectManager> proxyman(do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv));
  
  if (NS_FAILED(rv) || !proxyman) {
    return NS_ERROR_FAILURE;
  }
  
  NS_DEFINE_CID(nssComponentCID, NS_NSSCOMPONENT_CID);
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(nssComponentCID, &rv));

  if (NS_FAILED(rv) || !nssComponent) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIX509Cert> x509 = do_QueryInterface(this);
  if (!x509) {
    return NS_ERROR_NO_INTERFACE;
  }
  
  nsCOMPtr<nsIX509Cert> x509Proxy;
  proxyman->GetProxyForObject( NS_UI_THREAD_EVENTQ,
                               nsIX509Cert::GetIID(),
                               x509,
                               PROXY_SYNC | PROXY_ALWAYS,
                               getter_AddRefs(x509Proxy));

  if (!x509Proxy) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    rv = NS_OK;

    nsAutoString info;
    PRUnichar *temp1 = 0;

    nickWithSerial.Append(nickname);

    if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoIssuedFor").get(), info))) {
      details.Append(info);
      details.Append(NS_LITERAL_STRING("\n"));
    }

    if (NS_SUCCEEDED(x509Proxy->GetSubjectName(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
      details.Append(NS_LITERAL_STRING("  "));
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSubject").get(), info))) {
        details.Append(info);
        details.Append(NS_LITERAL_STRING(": "));
      }
      details.Append(temp1);
      nsMemory::Free(temp1);
      details.Append(NS_LITERAL_STRING("\n"));
    }

    if (NS_SUCCEEDED(x509Proxy->GetSerialNumber(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
      details.Append(NS_LITERAL_STRING("  "));
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSerialNo").get(), info))) {
        details.Append(info);
        details.Append(NS_LITERAL_STRING(": "));
      }
      details.Append(temp1);

      nickWithSerial.Append(NS_LITERAL_STRING(" ["));
      nickWithSerial.Append(temp1);
      nickWithSerial.Append(NS_LITERAL_STRING("]"));

      nsMemory::Free(temp1);
      details.Append(NS_LITERAL_STRING("\n"));
    }


    {
      nsCOMPtr<nsIX509CertValidity> validity;
      nsCOMPtr<nsIX509CertValidity> originalValidity;
      rv = x509Proxy->GetValidity(getter_AddRefs(originalValidity));
      if (NS_SUCCEEDED(rv) && originalValidity) {
        proxyman->GetProxyForObject( NS_UI_THREAD_EVENTQ,
                                     nsIX509CertValidity::GetIID(),
                                     originalValidity,
                                     PROXY_SYNC | PROXY_ALWAYS,
                                     getter_AddRefs(validity));
      }

      if (validity) {
        details.Append(NS_LITERAL_STRING("  "));
        if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoValid").get(), info))) {
          details.Append(info);
        }

        if (NS_SUCCEEDED(validity->GetNotBeforeLocalTime(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
          details.Append(NS_LITERAL_STRING(" "));
          if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoFrom").get(), info))) {
            details.Append(info);
          }
          details.Append(NS_LITERAL_STRING(" "));
          details.Append(temp1);
          nsMemory::Free(temp1);
        }

        if (NS_SUCCEEDED(validity->GetNotAfterLocalTime(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
          details.Append(NS_LITERAL_STRING(" "));
          if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoTo").get(), info))) {
            details.Append(info);
          }
          details.Append(NS_LITERAL_STRING(" "));
          details.Append(temp1);
          nsMemory::Free(temp1);
        }

        details.Append(NS_LITERAL_STRING("\n"));
      }
    }

    PRUint32 tempInt = 0;
    if (NS_SUCCEEDED(x509Proxy->GetPurposes(&tempInt, &temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
      details.Append(NS_LITERAL_STRING("  "));
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoPurposes").get(), info))) {
        details.Append(info);
      }
      details.Append(NS_LITERAL_STRING(": "));
      details.Append(temp1);
      nsMemory::Free(temp1);
      details.Append(NS_LITERAL_STRING("\n"));
    }

    if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertInfoIssuedBy").get(), info))) {
      details.Append(info);
      details.Append(NS_LITERAL_STRING("\n"));
    }

    if (NS_SUCCEEDED(x509Proxy->GetIssuerName(&temp1)) && temp1 && nsCharTraits<PRUnichar>::length(temp1)) {
      details.Append(NS_LITERAL_STRING("  "));
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSubject").get(), info))) {
        details.Append(info);
        details.Append(NS_LITERAL_STRING(": "));
      }
      details.Append(temp1);
      nsMemory::Free(temp1);
      details.Append(NS_LITERAL_STRING("\n"));
    }

    /*
      the above produces output the following output:

      Issued to: 
        Subject: $subjectName
        Serial number: $serialNumber
        Valid from: $starting_date to $expriation_date
        Purposes: $purposes
      Issued by:
        Subject: $issuerName
    */
  }
  
  return rv;
}


#define NS_NSS_LONG 4
#define NS_NSS_GET_LONG(x) ((((unsigned long)((x)[0])) << 24) | \
                            (((unsigned long)((x)[1])) << 16) | \
                            (((unsigned long)((x)[2])) <<  8) | \
                             ((unsigned long)((x)[3])) )
#define NS_NSS_PUT_LONG(src,dest) (dest)[0] = (((src) >> 24) & 0xff); \
                                  (dest)[1] = (((src) >> 16) & 0xff); \
                                  (dest)[2] = (((src) >>  8) & 0xff); \
                                  (dest)[3] = ((src) & 0xff); 


/* readonly attribute string dbKey; */
NS_IMETHODIMP 
nsNSSCertificate::GetDbKey(char * *aDbKey)
{
  SECItem key;

  NS_ENSURE_ARG(aDbKey);
  *aDbKey = nsnull;
  key.len = NS_NSS_LONG*4+mCert->serialNumber.len+mCert->derIssuer.len;
  key.data = (unsigned char *)nsMemory::Alloc(key.len);
  NS_NSS_PUT_LONG(0,key.data); // later put moduleID
  NS_NSS_PUT_LONG(0,&key.data[NS_NSS_LONG]); // later put slotID
  NS_NSS_PUT_LONG(mCert->serialNumber.len,&key.data[NS_NSS_LONG*2]);
  NS_NSS_PUT_LONG(mCert->derIssuer.len,&key.data[NS_NSS_LONG*3]);
  memcpy(&key.data[NS_NSS_LONG*4],mCert->serialNumber.data,
						mCert->serialNumber.len);
  memcpy(&key.data[NS_NSS_LONG*4+mCert->serialNumber.len],
			mCert->derIssuer.data, mCert->derIssuer.len);
  
  *aDbKey = NSSBase64_EncodeItem(nsnull, nsnull, 0, &key);
  nsMemory::Free(key.data); // SECItem is a 'c' type without a destrutor
  return (*aDbKey) ? NS_OK : NS_ERROR_FAILURE;
}

/* readonly attribute string windowTitle; */
NS_IMETHODIMP 
nsNSSCertificate::GetWindowTitle(char * *aWindowTitle)
{
  NS_ENSURE_ARG(aWindowTitle);
  if (mCert) {
    if (mCert->nickname) {
      *aWindowTitle = PL_strdup(mCert->nickname);
    } else {
      *aWindowTitle = CERT_GetCommonName(&mCert->subject);
      if (!*aWindowTitle) {
        *aWindowTitle = PL_strdup(mCert->subjectName);
      }
    }
  } else {
    NS_ASSERTION(0,"Somehow got nsnull for mCertificate in nsNSSCertificate.");
    *aWindowTitle = nsnull;
  }
  return NS_OK;
}

/*  readonly attribute wstring nickname; */
NS_IMETHODIMP
nsNSSCertificate::GetNickname(PRUnichar **_nickname)
{
  NS_ENSURE_ARG(_nickname);
  const char *nickname = (mCert->nickname) ? mCert->nickname : "(no nickname)";
  *_nickname = ToNewUnicode(NS_ConvertUTF8toUCS2(nickname));
  return NS_OK;
}

/*  readonly attribute wstring emailAddress; */
NS_IMETHODIMP
nsNSSCertificate::GetEmailAddress(PRUnichar **_emailAddress)
{
  NS_ENSURE_ARG(_emailAddress);
  const char *email = (mCert->emailAddr) ? mCert->emailAddr : "(no email address)";
  *_emailAddress = ToNewUnicode(NS_ConvertUTF8toUCS2(email));
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetCommonName(PRUnichar **aCommonName)
{
  NS_ENSURE_ARG(aCommonName);
  *aCommonName = nsnull;
  if (mCert) {
    char *commonName = CERT_GetCommonName(&mCert->subject);
    if (commonName) {
      *aCommonName = ToNewUnicode(NS_ConvertUTF8toUCS2(commonName));
      PORT_Free(commonName);
    } /*else {
      *aCommonName = ToNewUnicode(NS_LITERAL_STRING("<not set>")), 
    }*/
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetOrganization(PRUnichar **aOrganization)
{
  NS_ENSURE_ARG(aOrganization);
  *aOrganization = nsnull;
  if (mCert) {
    char *organization = CERT_GetOrgName(&mCert->subject);
    if (organization) {
      *aOrganization = ToNewUnicode(NS_ConvertUTF8toUCS2(organization));
      PORT_Free(organization);
    } /*else {
      *aOrganization = ToNewUnicode(NS_LITERAL_STRING("<not set>")), 
    }*/
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerCommonName(PRUnichar **aCommonName)
{
  NS_ENSURE_ARG(aCommonName);
  *aCommonName = nsnull;
  if (mCert) {
    char *commonName = CERT_GetCommonName(&mCert->issuer);
    if (commonName) {
      *aCommonName = ToNewUnicode(NS_ConvertUTF8toUCS2(commonName));
      PORT_Free(commonName);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerOrganization(PRUnichar **aOrganization)
{
  NS_ENSURE_ARG(aOrganization);
  *aOrganization = nsnull;
  if (mCert) {
    char *organization = CERT_GetOrgName(&mCert->issuer);
    if (organization) {
      *aOrganization = ToNewUnicode(NS_ConvertUTF8toUCS2(organization));
      PORT_Free(organization);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerOrganizationUnit(PRUnichar **aOrganizationUnit)
{
  NS_ENSURE_ARG(aOrganizationUnit);
  *aOrganizationUnit = nsnull;
  if (mCert) {
    char *organizationUnit = CERT_GetOrgUnitName(&mCert->issuer);
    if (organizationUnit) {
      *aOrganizationUnit = ToNewUnicode(NS_ConvertUTF8toUCS2(organizationUnit));
      PORT_Free(organizationUnit);
    }
  }
  return NS_OK;
}

/* readonly attribute nsIX509Cert issuer; */
NS_IMETHODIMP 
nsNSSCertificate::GetIssuer(nsIX509Cert * *aIssuer)
{
  NS_ENSURE_ARG(aIssuer);
  *aIssuer = nsnull;
  CERTCertificate *issuer;
  issuer = CERT_FindCertIssuer(mCert, PR_Now(), certUsageSSLClient);
  if (issuer) {
    nsCOMPtr<nsIX509Cert> cert = new nsNSSCertificate(issuer);
    *aIssuer = cert;
    NS_ADDREF(*aIssuer);
    CERT_DestroyCertificate(issuer);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetOrganizationalUnit(PRUnichar **aOrganizationalUnit)
{
  NS_ENSURE_ARG(aOrganizationalUnit);
  *aOrganizationalUnit = nsnull;
  if (mCert) {
    char *orgunit = CERT_GetOrgUnitName(&mCert->subject);
    if (orgunit) {
      *aOrganizationalUnit = ToNewUnicode(NS_ConvertUTF8toUCS2(orgunit));
      PORT_Free(orgunit);
    } /*else {
      *aOrganizationalUnit = ToNewUnicode(NS_LITERAL_STRING("<not set>")), 
    }*/
  }
  return NS_OK;
}

/* 
 * nsIEnumerator getChain(); 
 */
NS_IMETHODIMP
nsNSSCertificate::GetChain(nsISupportsArray **_rvChain)
{
  NS_ENSURE_ARG(_rvChain);
  nsresult rv;
  /* Get the cert chain from NSS */
  CERTCertList *nssChain = NULL;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting chain for \"%s\"\n", mCert->nickname));
  // XXX This function is buggy - if it can't find the issuer, it crashes
  //     on a null pointer.  Will have to wait until it is fixed in NSS.
#ifdef NSS_CHAIN_BUG_FIXED
  nssChain = CERT_GetCertChainFromCert(mCert, PR_Now(), certUsageSSLClient);
  if (!nssChain)
    return NS_ERROR_FAILURE;
  /* enumerate the chain for scripting purposes */
  nsCOMPtr<nsISupportsArray> array;
  rv = NS_NewISupportsArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) { 
    goto done; 
  }
  for (node = CERT_LIST_HEAD(nssChain);
       !CERT_LIST_END(node, nssChain);
       node = CERT_LIST_NEXT(node)) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("adding %s to chain\n", node->cert->nickname));
    nsCOMPtr<nsIX509Cert> cert = new nsNSSCertificate(node->cert);
    array->AppendElement(cert);
  }
#else // workaround here
  CERTCertificate *cert = nsnull;
  /* enumerate the chain for scripting purposes */
  nsCOMPtr<nsISupportsArray> array;
  rv = NS_NewISupportsArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) { 
    goto done; 
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting chain for \"%s\"\n", mCert->nickname));
  cert = CERT_DupCertificate(mCert);
  while (cert) {
    nsCOMPtr<nsIX509Cert> pipCert = new nsNSSCertificate(cert);
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("adding %s to chain\n", cert->nickname));
    array->AppendElement(pipCert);
    PRBool wantToBreak = PR_FALSE;
    CERTCertificate *next_cert = nsnull;
    if (SECITEM_CompareItem(&cert->derIssuer, &cert->derSubject) == SECEqual) {
      wantToBreak = PR_TRUE;
    }
    else {
      next_cert = CERT_FindCertIssuer(cert, PR_Now(), certUsageSSLClient);
    }
    CERT_DestroyCertificate(cert);
    if (wantToBreak) {
      break;
    }
    cert = next_cert;
  }
#endif // NSS_CHAIN_BUG_FIXED
  *_rvChain = array;
  NS_IF_ADDREF(*_rvChain);
  rv = NS_OK;
done:
  if (nssChain)
    CERT_DestroyCertList(nssChain);
  return rv;
}

/*  readonly attribute wstring subjectName; */
NS_IMETHODIMP
nsNSSCertificate::GetSubjectName(PRUnichar **_subjectName)
{
  NS_ENSURE_ARG(_subjectName);
  *_subjectName = nsnull;
  if (mCert->subjectName) {
    *_subjectName = ToNewUnicode(NS_ConvertUTF8toUCS2(mCert->subjectName));
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring issuerName; */
NS_IMETHODIMP
nsNSSCertificate::GetIssuerName(PRUnichar **_issuerName)
{
  NS_ENSURE_ARG(_issuerName);
  *_issuerName = nsnull;
  if (mCert->issuerName) {
    *_issuerName = ToNewUnicode(NS_ConvertUTF8toUCS2(mCert->issuerName));
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring serialNumber; */
NS_IMETHODIMP
nsNSSCertificate::GetSerialNumber(PRUnichar **_serialNumber)
{
  NS_ENSURE_ARG(_serialNumber);
  *_serialNumber = nsnull;
  nsXPIDLCString tmpstr; tmpstr.Adopt(CERT_Hexify(&mCert->serialNumber, 1));
  if (tmpstr.get()) {
    *_serialNumber = ToNewUnicode(tmpstr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring rsaPubModulus; */
NS_IMETHODIMP
nsNSSCertificate::GetRsaPubModulus(PRUnichar **_rsaPubModulus)
{
  NS_ENSURE_ARG(_rsaPubModulus);
  *_rsaPubModulus = nsnull;
  //nsXPIDLCString tmpstr; tmpstr.Adopt(CERT_Hexify(&mCert->serialNumber, 1));
  NS_NAMED_LITERAL_CSTRING(tmpstr, "not yet implemented");
  if (tmpstr.get()) {
    *_rsaPubModulus = ToNewUnicode(tmpstr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring sha1Fingerprint; */
NS_IMETHODIMP
nsNSSCertificate::GetSha1Fingerprint(PRUnichar **_sha1Fingerprint)
{
  NS_ENSURE_ARG(_sha1Fingerprint);
  *_sha1Fingerprint = nsnull;
  unsigned char fingerprint[20];
  SECItem fpItem;
  memset(fingerprint, 0, sizeof fingerprint);
  PK11_HashBuf(SEC_OID_SHA1, fingerprint, 
               mCert->derCert.data, mCert->derCert.len);
  fpItem.data = fingerprint;
  fpItem.len = SHA1_LENGTH;
  nsXPIDLCString fpStr; fpStr.Adopt(CERT_Hexify(&fpItem, 1));
  if (fpStr.get()) {
    *_sha1Fingerprint = ToNewUnicode(fpStr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring md5Fingerprint; */
NS_IMETHODIMP
nsNSSCertificate::GetMd5Fingerprint(PRUnichar **_md5Fingerprint)
{
  NS_ENSURE_ARG(_md5Fingerprint);
  *_md5Fingerprint = nsnull;
  unsigned char fingerprint[20];
  SECItem fpItem;
  memset(fingerprint, 0, sizeof fingerprint);
  PK11_HashBuf(SEC_OID_MD5, fingerprint, 
               mCert->derCert.data, mCert->derCert.len);
  fpItem.data = fingerprint;
  fpItem.len = MD5_LENGTH;
  nsXPIDLCString fpStr; fpStr.Adopt(CERT_Hexify(&fpItem, 1));
  if (fpStr.get()) {
    *_md5Fingerprint = ToNewUnicode(fpStr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/* readonly attribute wstring issuedDate; */
NS_IMETHODIMP
nsNSSCertificate::GetIssuedDate(PRUnichar **_issuedDate)
{
  NS_ENSURE_ARG(_issuedDate);
  *_issuedDate = nsnull;
  nsresult rv;
  PRTime beforeTime;
  nsCOMPtr<nsIX509CertValidity> validity;
  rv = this->GetValidity(getter_AddRefs(validity));
  if (NS_FAILED(rv)) return rv;
  rv = validity->GetNotBefore(&beforeTime);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsAutoString date;
  dateFormatter->FormatPRTime(nsnull, kDateFormatShort, kTimeFormatNone,
                              beforeTime, date);
  *_issuedDate = ToNewUnicode(date);
  return NS_OK;
}

/* readonly attribute wstring expiresDate; */
NS_IMETHODIMP
nsNSSCertificate::GetExpiresDate(PRUnichar **_expiresDate)
{
  NS_ENSURE_ARG(_expiresDate);
  *_expiresDate = nsnull;
  nsresult rv;
  PRTime afterTime;
  nsCOMPtr<nsIX509CertValidity> validity;
  rv = this->GetValidity(getter_AddRefs(validity));
  if (NS_FAILED(rv)) return rv;
  rv = validity->GetNotAfter(&afterTime);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsAutoString date;
  dateFormatter->FormatPRTime(nsnull, kDateFormatShort, kTimeFormatNone,
                              afterTime, date);
  *_expiresDate = ToNewUnicode(date);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetTokenName(PRUnichar **aTokenName)
{
  NS_ENSURE_ARG(aTokenName);
  *aTokenName = nsnull;
  if (mCert) {
    // HACK alert
    // When the trust of a builtin cert is modified, NSS copies it into the
    // cert db.  At this point, it is now "managed" by the user, and should
    // not be listed with the builtins.  However, in the collection code
    // used by PK11_ListCerts, the cert is found in the temp db, where it
    // has been loaded from the token.  Though the trust is correct (grabbed
    // from the cert db), the source is wrong.  I believe this is a safe
    // way to work around this.
    if (mCert->slot) {
      char *token = PK11_GetTokenName(mCert->slot);
      if (token) {
        *aTokenName = ToNewUnicode(NS_ConvertUTF8toUCS2(token));
      }
    } else {
      nsresult rv;
      nsAutoString tok;
      nsCOMPtr<nsINSSComponent> nssComponent(
		                        do_GetService(kNSSComponentCID, &rv));
      if (NS_FAILED(rv)) return rv;
      rv = nssComponent->GetPIPNSSBundleString(
                                NS_LITERAL_STRING("InternalToken").get(), tok);
      if (NS_SUCCEEDED(rv))
        *aTokenName = ToNewUnicode(tok);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetUsesOCSP(PRBool *aUsesOCSP)
{
  nsCOMPtr<nsIPref> prefService = do_GetService(NS_PREF_CONTRACTID);
 
  PRInt32 ocspEnabled; 
  prefService->GetIntPref("security.OCSP.enabled", &ocspEnabled);
  if (ocspEnabled == 2) {
    *aUsesOCSP = PR_TRUE;
  } else if (ocspEnabled == 1) {
    nsXPIDLCString ocspLocation;
    ocspLocation.Adopt(CERT_GetOCSPAuthorityInfoAccessLocation(mCert));
    *aUsesOCSP = (ocspLocation) ? PR_TRUE : PR_FALSE;
  } else {
    *aUsesOCSP = PR_FALSE;
  }
  return NS_OK;
}

/* [noscript] long getRawDER (out charPtr result) */
NS_IMETHODIMP
nsNSSCertificate::GetRawDER(char **result, PRUint32 *_retval)
{
  if (mCert) {
    *result = (char *)mCert->derCert.data;
    *_retval = mCert->derCert.len;
    return NS_OK;
  }
  *_retval = 0;
  return NS_ERROR_FAILURE;
}

CERTCertificate *
nsNSSCertificate::GetCert()
{
  return (mCert) ? CERT_DupCertificate(mCert) : nsnull;
}

NS_IMETHODIMP
nsNSSCertificate::GetValidity(nsIX509CertValidity **aValidity)
{
  NS_ENSURE_ARG(aValidity);
  nsX509CertValidity *validity = new nsX509CertValidity(mCert);
  if (nsnull == validity)
   return  NS_ERROR_OUT_OF_MEMORY; 

  NS_ADDREF(validity);
  *aValidity = NS_STATIC_CAST(nsIX509CertValidity*, validity);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::VerifyForUsage(PRUint32 usage, PRUint32 *verificationResult)
{
  NS_ENSURE_ARG(verificationResult);

  SECCertUsage nss_usage;
  
  switch (usage)
  {
    case CERT_USAGE_SSLClient:
      nss_usage = certUsageSSLClient;
      break;

    case CERT_USAGE_SSLServer:
      nss_usage = certUsageSSLServer;
      break;

    case CERT_USAGE_SSLServerWithStepUp:
      nss_usage = certUsageSSLServerWithStepUp;
      break;

    case CERT_USAGE_SSLCA:
      nss_usage = certUsageSSLCA;
      break;

    case CERT_USAGE_EmailSigner:
      nss_usage = certUsageEmailSigner;
      break;

    case CERT_USAGE_EmailRecipient:
      nss_usage = certUsageEmailRecipient;
      break;

    case CERT_USAGE_ObjectSigner:
      nss_usage = certUsageObjectSigner;
      break;

    case CERT_USAGE_UserCertImport:
      nss_usage = certUsageUserCertImport;
      break;

    case CERT_USAGE_VerifyCA:
      nss_usage = certUsageVerifyCA;
      break;

    case CERT_USAGE_ProtectedObjectSigner:
      nss_usage = certUsageProtectedObjectSigner;
      break;

    case CERT_USAGE_StatusResponder:
      nss_usage = certUsageStatusResponder;
      break;

    case CERT_USAGE_AnyCA:
      nss_usage = certUsageAnyCA;
      break;

    default:
      return NS_ERROR_FAILURE;
  }

  CERTCertDBHandle *defaultcertdb = CERT_GetDefaultCertDB();

  if (CERT_VerifyCertNow(defaultcertdb, mCert, PR_TRUE, 
                         nss_usage, NULL) == SECSuccess)
  {
    *verificationResult = VERIFIED_OK;
  }
  else
  {
    int err = PR_GetError();

    // this list was cloned from verifyFailed

    switch (err)
    {
      case SEC_ERROR_INADEQUATE_KEY_USAGE:
      case SEC_ERROR_INADEQUATE_CERT_TYPE:
        *verificationResult = USAGE_NOT_ALLOWED;
        break;

      case SEC_ERROR_REVOKED_CERTIFICATE:
        *verificationResult = CERT_REVOKED;
        break;

      case SEC_ERROR_EXPIRED_CERTIFICATE:
        *verificationResult = CERT_EXPIRED;
        break;
        
      case SEC_ERROR_UNTRUSTED_CERT:
        *verificationResult = CERT_NOT_TRUSTED;
        break;
        
      case SEC_ERROR_UNTRUSTED_ISSUER:
        *verificationResult = ISSUER_NOT_TRUSTED;
        break;
        
      case SEC_ERROR_UNKNOWN_ISSUER:
        *verificationResult = ISSUER_UNKNOWN;
        break;
        
      case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
        *verificationResult = INVALID_CA;
        break;
        
      case SEC_ERROR_CERT_USAGES_INVALID:
      default:
        *verificationResult = NOT_VERIFIED_UNKNOWN; 
        break;
    }
  }
  
  return NS_OK;  
}

class UsageArrayHelper
{
public:
  UsageArrayHelper(CERTCertificate *aCert);

  nsresult GetUsageArray(char *suffix,
               PRUint32 outArraySize,
               PRUint32 *_verified,
               PRUint32 *_count,
               PRUnichar **tmpUsages);

  enum { max_returned_out_array_size = 12 };

private:
  CERTCertificate *mCert;
  nsresult m_rv;
  CERTCertDBHandle *defaultcertdb;
  nsCOMPtr<nsINSSComponent> nssComponent;
  int mCached_NonInadequateReason;

  void check(char *suffix,
             SECCertUsage aCertUsage,
             PRUint32 &aCounter,
             PRUnichar **outUsages);

  void verifyFailed(PRUint32 *_verified);
};

UsageArrayHelper::UsageArrayHelper(CERTCertificate *aCert)
:mCert(aCert)
{
  defaultcertdb = CERT_GetDefaultCertDB();
  nssComponent = do_GetService(kNSSComponentCID, &m_rv);
  mCached_NonInadequateReason = SECSuccess;
}

void
UsageArrayHelper::check(char *suffix,
                        SECCertUsage aCertUsage,
                        PRUint32 &aCounter,
                        PRUnichar **outUsages)
{
  if (CERT_VerifyCertNow(defaultcertdb, mCert, PR_TRUE, 
                         aCertUsage, NULL) == SECSuccess) {
    nsAutoString typestr;
    switch (aCertUsage) {
      case certUsageSSLClient:
        typestr = NS_LITERAL_STRING("VerifySSLClient");
        break;
      case certUsageSSLServer:
        typestr = NS_LITERAL_STRING("VerifySSLServer");
        break;
      case certUsageSSLServerWithStepUp:
        typestr = NS_LITERAL_STRING("VerifySSLStepUp");
        break;
      case certUsageEmailSigner:
        typestr = NS_LITERAL_STRING("VerifyEmailSigner");
        break;
      case certUsageEmailRecipient:
        typestr = NS_LITERAL_STRING("VerifyEmailRecip");
        break;
      case certUsageObjectSigner:
        typestr = NS_LITERAL_STRING("VerifyObjSign");
        break;
      case certUsageProtectedObjectSigner:
        typestr = NS_LITERAL_STRING("VerifyProtectObjSign");
        break;
      case certUsageUserCertImport:
        typestr = NS_LITERAL_STRING("VerifyUserImport");
        break;
      case certUsageSSLCA:
        typestr = NS_LITERAL_STRING("VerifySSLCA");
        break;
      case certUsageVerifyCA:
        typestr = NS_LITERAL_STRING("VerifyCAVerifier");
        break;
      case certUsageStatusResponder:
        typestr = NS_LITERAL_STRING("VerifyStatusResponder");
        break;
      case certUsageAnyCA:
        typestr = NS_LITERAL_STRING("VerifyAnyCA");
        break;
      default:
        break;
    }
    if (!typestr.IsEmpty()) {
      typestr.AppendWithConversion(suffix);
      nsAutoString verifyDesc;
      m_rv = nssComponent->GetPIPNSSBundleString(typestr.get(), verifyDesc);
      if (NS_SUCCEEDED(m_rv)) {
        outUsages[aCounter++] = ToNewUnicode(verifyDesc);
      }
    }
  }
  else {
    int err = PR_GetError();
    
    if (SECSuccess == mCached_NonInadequateReason) {
      // we have not yet cached anything
      mCached_NonInadequateReason = err;
    }
    else {
      switch (err) {
        case SEC_ERROR_INADEQUATE_KEY_USAGE:
        case SEC_ERROR_INADEQUATE_CERT_TYPE:
          // this code should not override a possibly cached more informative reason
          break;
        
        default:
          mCached_NonInadequateReason = err;
          break;
      }
    }
  }
}

void
UsageArrayHelper::verifyFailed(PRUint32 *_verified)
{
  switch (mCached_NonInadequateReason) {
  /* For these cases, verify only failed for the particular usage */
  case SEC_ERROR_INADEQUATE_KEY_USAGE:
  case SEC_ERROR_INADEQUATE_CERT_TYPE:
    *_verified = nsNSSCertificate::USAGE_NOT_ALLOWED; break;
  /* These are the cases that have individual error messages */
  case SEC_ERROR_REVOKED_CERTIFICATE:
    *_verified = nsNSSCertificate::CERT_REVOKED; break;
  case SEC_ERROR_EXPIRED_CERTIFICATE:
    *_verified = nsNSSCertificate::CERT_EXPIRED; break;
  case SEC_ERROR_UNTRUSTED_CERT:
    *_verified = nsNSSCertificate::CERT_NOT_TRUSTED; break;
  case SEC_ERROR_UNTRUSTED_ISSUER:
    *_verified = nsNSSCertificate::ISSUER_NOT_TRUSTED; break;
  case SEC_ERROR_UNKNOWN_ISSUER:
    *_verified = nsNSSCertificate::ISSUER_UNKNOWN; break;
  case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
    // XXX are there other error for this?
    *_verified = nsNSSCertificate::INVALID_CA; break;
  case SEC_ERROR_CERT_USAGES_INVALID: // XXX what is this?
  // there are some OCSP errors from PSM 1.x to add here
  case SECSuccess:
    // this means, no verification result has ever been received
  default:
    *_verified = nsNSSCertificate::NOT_VERIFIED_UNKNOWN; break;
  }
}

nsresult
UsageArrayHelper::GetUsageArray(char *suffix,
                      PRUint32 outArraySize,
                      PRUint32 *_verified,
                      PRUint32 *_count,
                      PRUnichar **outUsages)
{
  if (NS_FAILED(m_rv))
    return m_rv;

  if (outArraySize < max_returned_out_array_size)
    return NS_ERROR_FAILURE;

  PRUint32 &count = *_count;
  count = 0;
  
  // The following list of checks must be < max_returned_out_array_size
  
  check(suffix, certUsageSSLClient, count, outUsages);
  check(suffix, certUsageSSLServer, count, outUsages);
  check(suffix, certUsageSSLServerWithStepUp, count, outUsages);
  check(suffix, certUsageEmailSigner, count, outUsages);
  check(suffix, certUsageEmailRecipient, count, outUsages);
  check(suffix, certUsageObjectSigner, count, outUsages);
#if 0
  check(suffix, certUsageProtectedObjectSigner, count, outUsages);
  check(suffix, certUsageUserCertImport, count, outUsages);
#endif
  check(suffix, certUsageSSLCA, count, outUsages);
#if 0
  check(suffix, certUsageVerifyCA, count, outUsages);
#endif
  check(suffix, certUsageStatusResponder, count, outUsages);
#if 0
  check(suffix, certUsageAnyCA, count, outUsages);
#endif
  if (count == 0) {
    verifyFailed(_verified);
  } else {
    *_verified = nsNSSCertificate::VERIFIED_OK;
  }
  return NS_OK;
}

static nsresult
GetIntValue(SECItem *versionItem, 
            unsigned long *version)
{
  SECStatus srv;

  srv = SEC_ASN1DecodeInteger(versionItem,version);
  if (srv != SECSuccess) {
    NS_ASSERTION(0,"Could not decode version of cert");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

static nsresult
ProcessVersion(SECItem         *versionItem,
               nsINSSComponent *nssComponent,
               nsIASN1PrintableItem **retItem)
{
  nsresult rv;
  nsString text;
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
 
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpVersion").get(),
                                      text);
  rv = printableItem->SetDisplayName(text.get());
  if (NS_FAILED(rv))
    return rv;

  // Now to figure out what version this certificate is.
  unsigned long version;

  if (versionItem->data) {
    rv = GetIntValue(versionItem, &version);
    if (NS_FAILED(rv))
      return rv;
  } else {
    // If there is no version present in the cert, then rfc2459
    // says we default to v1 (0)
    version = 0;
  }

  switch (version){
  case 0:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpVersion1").get(),
                                             text);
    break;
  case 1:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpVersion2").get(),
                                             text);
    break;
  case 2:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpVersion3").get(),
                                             text);
    break;
  default:
    NS_ASSERTION(0,"Bad value for cert version");
    rv = NS_ERROR_FAILURE;
  }
    
  if (NS_FAILED(rv))
    return rv;

  rv = printableItem->SetDisplayValue(text.get());
  if (NS_FAILED(rv))
    return rv;

  *retItem = printableItem;
  NS_ADDREF(*retItem);
  return NS_OK;
}

nsresult 
ProcessSerialNumberDER(SECItem         *serialItem, 
                       nsINSSComponent *nssComponent,
                       nsIASN1PrintableItem **retItem)
{
  nsresult rv;
  nsString text;
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();

  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSerialNo").get(),
                                           text); 
  if (NS_FAILED(rv))
    return rv;

  rv = printableItem->SetDisplayName(text.get());
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString serialNumber;
  serialNumber.Adopt(CERT_Hexify(serialItem, 1));
  if (serialNumber == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = printableItem->SetDisplayValue(NS_ConvertASCIItoUCS2(serialNumber).get());
  *retItem = printableItem;
  NS_ADDREF(*retItem);
  return rv;
}

static nsresult
GetDefaultOIDFormat(SECItem *oid,
                    nsString &outString)
{
  char buf[300];
  unsigned int len;
  int written;
    
  unsigned long val  = oid->data[0];
  unsigned int  i    = val % 40;
  val /= 40;
  written = PR_snprintf(buf, 300, "%lu %u ", val, i);
  if (written < 0)
    return NS_ERROR_FAILURE;	
  len = written;

  val = 0;
  for (i = 1; i < oid->len; ++i) {
    // In this loop, we have to parse a DER formatted 
    // If the first bit is a 1, then the integer is 
    // represented by more than one byte.  If the 
    // first bit is set then we continue on and add
    // the values of the later bytes until we get 
    // a byte without the first bit set.
    unsigned long j;

    j = oid->data[i];
    val = (val << 7) | (j & 0x7f);
    if (j & 0x80)
      continue;
    written = PR_snprintf(&buf[len], sizeof(buf)-len, "%lu ", val);
    if (written < 0)
      return NS_ERROR_FAILURE;

    len += written;
    NS_ASSERTION(len < sizeof(buf), "OID data to big to display in 300 chars.");
    val = 0;      
  }

  outString = NS_ConvertASCIItoUCS2(buf).get();
  return NS_OK; 
}

static nsresult
GetOIDText(SECItem *oid, nsINSSComponent *nssComponent, nsString &text)
{ 
  nsresult rv;
  SECOidTag oidTag = SECOID_FindOIDTag(oid);

  switch (oidTag) {
  case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpMD2WithRSA").get(),
                                             text);
    break;
  case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpMD5WithRSA").get(),
                                             text);
    break;
  case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSHA1WithRSA").get(),
                                             text);
    break;
  case SEC_OID_AVA_COUNTRY_NAME:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVACountry").get(),
                                             text);
    break;
  case SEC_OID_AVA_COMMON_NAME:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVACN").get(),
                                             text);
    break;
  case SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVAOU").get(),
                                             text);
    break;
  case SEC_OID_AVA_ORGANIZATION_NAME:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVAOrg").get(),
                                             text);
    break;
  case SEC_OID_AVA_LOCALITY:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVALocality").get(),
                                             text);
    break;
  case SEC_OID_AVA_DN_QUALIFIER:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVADN").get(),
                                             text);
    break;
  case SEC_OID_AVA_DC:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVADC").get(),
                                             text);
    break;
  case SEC_OID_AVA_STATE_OR_PROVINCE:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAVAState").get(),
                                             text);
    break;
  case SEC_OID_PKCS1_RSA_ENCRYPTION:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpRSAEncr").get(),
                                             text);
    break;
  case SEC_OID_X509_KEY_USAGE:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKeyUsage").get(),
                                             text);
    break;
  case SEC_OID_NS_CERT_EXT_CERT_TYPE:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpCertType").get(),
                                             text);
    break;
  case SEC_OID_X509_AUTH_KEY_ID:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAuthKeyID").get(),
                                             text);
    break;
  case SEC_OID_RFC1274_UID:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpUserID").get(),
                                             text);
    break;
  case SEC_OID_PKCS9_EMAIL_ADDRESS:
    rv = nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpPK9Email").get(),
                                             text);
    break;
  default:
    rv = GetDefaultOIDFormat(oid, text);
    if (NS_FAILED(rv))
      return rv;

    const PRUnichar *params[1] = {text.get()};
    nsXPIDLString text2;
    rv = nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("CertDumpDefOID").get(),
                                                     params, 1,
                                                     getter_Copies(text2));
    text = text2;
    break;
  }
  return rv;  
}

#define SEPARATOR "\n"

static nsresult
ProcessRawBytes(SECItem *data, nsString &text)
{
  // This function is used to display some DER bytes
  // that we have not added support for decoding.
  // It prints the value of the byte out into a 
  // string that can later be displayed as a byte
  // string.  We place a new line after 24 bytes
  // to break up extermaly long sequence of bytes.
  PRUint32 i;
  char buffer[5];
  for (i=0; i<data->len; i++) {
    PR_snprintf(buffer, 5, "%02x ", data->data[i]);
    text.Append(NS_ConvertASCIItoUCS2(buffer).get());
    if ((i+1)%16 == 0) {
      text.Append(NS_LITERAL_STRING(SEPARATOR).get());
    }
  }
  return NS_OK;
}    

static nsresult
ProcessNSCertTypeExtensions(SECItem  *extData, 
                            nsString &text,
                            nsINSSComponent *nssComponent)
{
  SECItem decoded;
  decoded.data = nsnull;
  decoded.len  = 0;
  SEC_ASN1DecodeItem(nsnull, &decoded, 
		SEC_ASN1_GET(SEC_BitStringTemplate), extData);
  unsigned char nsCertType = decoded.data[0];
  nsString local;
  nsMemory::Free(decoded.data);
  if (nsCertType & NS_CERT_TYPE_SSL_CLIENT) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifySSLClient").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_SSL_SERVER) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifySSLServer").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_EMAIL) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpCertTypeEmail").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_OBJECT_SIGNING) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifyObjSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_SSL_CA) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifySSLCA").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_EMAIL_CA) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpEmailCA").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (nsCertType & NS_CERT_TYPE_OBJECT_SIGNING_CA) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("VerifyObjSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  return NS_OK;
}

static nsresult
ProcessKeyUsageExtension(SECItem *extData, nsString &text,
                         nsINSSComponent *nssComponent)
{
  SECItem decoded;
  decoded.data = nsnull;
  decoded.len  = 0;
  SEC_ASN1DecodeItem(nsnull, &decoded, 
				SEC_ASN1_GET(SEC_BitStringTemplate), extData);
  unsigned char keyUsage = decoded.data[0];
  nsString local;
  nsMemory::Free(decoded.data);  
  if (keyUsage & KU_DIGITAL_SIGNATURE) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_NON_REPUDIATION) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUNonRep").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_KEY_ENCIPHERMENT) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUEnc").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_DATA_ENCIPHERMENT) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUDEnc").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_KEY_AGREEMENT) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUKA").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_KEY_CERT_SIGN) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUCertSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }
  if (keyUsage & KU_CRL_SIGN) {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpKUCRLSign").get(),
                                        local);
    text.Append(local.get());
    text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  }

  return NS_OK;
}

static nsresult
ProcessExtensionData(SECOidTag oidTag, SECItem *extData, 
                     nsString &text, nsINSSComponent *nssComponent)
{
  nsresult rv;
  switch (oidTag) {
  case SEC_OID_NS_CERT_EXT_CERT_TYPE:
    rv = ProcessNSCertTypeExtensions(extData, text, nssComponent);
    break;
  case SEC_OID_X509_KEY_USAGE:
    rv = ProcessKeyUsageExtension(extData, text, nssComponent);
    break;
  default:
    rv = ProcessRawBytes(extData, text);
    break; 
  }
  return rv;
}

static nsresult
ProcessSingleExtension(CERTCertExtension *extension,
                       nsINSSComponent *nssComponent,
                       nsIASN1PrintableItem **retExtension)
{
  nsString text;
  GetOIDText(&extension->id, nssComponent, text);
  nsCOMPtr<nsIASN1PrintableItem>extensionItem = new nsNSSASN1PrintableItem();
  if (extensionItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  extensionItem->SetDisplayName(text.get());
  SECOidTag oidTag = SECOID_FindOIDTag(&extension->id);
  text.Truncate();
  if (extension->critical.data != nsnull) {
    if (extension->critical.data[0]) {
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpCritical").get(),
                                          text);
    } else {
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpNonCritical").get(),
                                         text);
    }
  } else {
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpNonCritical").get(),
                                        text);
  }
  text.Append(NS_LITERAL_STRING(SEPARATOR).get());
  nsresult rv = ProcessExtensionData(oidTag, &extension->value, text, 
                                     nssComponent);
  if (NS_FAILED(rv))
    return rv;

  extensionItem->SetDisplayValue(text.get());
  *retExtension = extensionItem;
  NS_ADDREF(*retExtension);
  return NS_OK;
}

#ifdef DEBUG_javi
void
DumpASN1Object(nsIASN1Object *object, unsigned int level)
{
  PRUnichar *dispNameU, *dispValU;
  unsigned int i;
  nsCOMPtr<nsISupportsArray> asn1Objects;
  nsCOMPtr<nsISupports> isupports;
  nsCOMPtr<nsIASN1Object> currObject;
  PRBool processObjects;
  PRUint32 numObjects;

  for (i=0; i<level; i++)
    printf ("  ");

  object->GetDisplayName(&dispNameU);
  nsCOMPtr<nsIASN1Sequence> sequence(do_QueryInterface(object));
  if (sequence) {
    printf ("%s ", NS_ConvertUCS2toUTF8(dispNameU).get());
    sequence->GetProcessObjects(&processObjects);
    if (processObjects) {
      printf("\n");
      sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
      asn1Objects->Count(&numObjects);
      for (i=0; i<numObjects;i++) {
        isupports = dont_AddRef(asn1Objects->ElementAt(i));
        currObject = do_QueryInterface(isupports);
        DumpASN1Object(currObject, level+1);    
      }
    } else { 
      object->GetDisplayValue(&dispValU);
      printf("= %s\n", NS_ConvertUCS2toUTF8(dispValU).get()); 
      PR_Free(dispValU);
    }
  } else { 
    object->GetDisplayValue(&dispValU);
    printf("%s = %s\n",NS_ConvertUCS2toUTF8(dispNameU).get(), 
                       NS_ConvertUCS2toUTF8(dispValU).get()); 
    PR_Free(dispValU);
  }
  PR_Free(dispNameU);
}
#endif

/*
 * void getUsages(out PRUint32 verified,
 *                out PRUint32 count, 
 *                [retval, array, size_is(count)] out wstring usages);
 */
NS_IMETHODIMP
nsNSSCertificate::GetUsages(PRUint32 *_verified,
                            PRUint32 *_count,
                            PRUnichar ***_usages)
{
  nsresult rv;
  PRUnichar *tmpUsages[13];
  char *suffix = "";
  PRUint32 tmpCount;
  UsageArrayHelper uah(mCert);
  rv = uah.GetUsageArray(suffix, 13, _verified, &tmpCount, tmpUsages);
  if (tmpCount > 0) {
    *_usages = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * tmpCount);
    for (PRUint32 i=0; i<tmpCount; i++) {
      (*_usages)[i] = tmpUsages[i];
    }
    *_count = tmpCount;
    return NS_OK;
  }
  *_usages = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *));
  *_count = 0;
  return NS_OK;
}

/* void getPurposes(out PRUint32 verified, out wstring purposes); */
NS_IMETHODIMP
nsNSSCertificate::GetPurposes(PRUint32   *_verified,
                              PRUnichar **_purposes)
{
  nsresult rv;
  PRUnichar *tmpUsages[13];
  char *suffix = "_p";
  PRUint32 tmpCount;
  UsageArrayHelper uah(mCert);
  rv = uah.GetUsageArray(suffix, 13, _verified, &tmpCount, tmpUsages);
  nsAutoString porpoises;
  for (PRUint32 i=0; i<tmpCount; i++) {
    if (i>0) porpoises.Append(NS_LITERAL_STRING(","));
    porpoises.Append(tmpUsages[i]);
    nsMemory::Free(tmpUsages[i]);
  }
  if (_purposes != NULL) {  // skip it for verify-only
    *_purposes = ToNewUnicode(porpoises);
  }
  return NS_OK;
}

/* void view (); */
NS_IMETHODIMP
nsNSSCertificate::View()
{
  nsresult rv;

  nsCOMPtr<nsICertificateDialogs> certDialogs;
  rv = ::getNSSDialogs(getter_AddRefs(certDialogs),
                       NS_GET_IID(nsICertificateDialogs));
  return certDialogs->ViewCert(this);
}

static nsresult
ProcessSECAlgorithmID(SECAlgorithmID *algID,
                      nsINSSComponent *nssComponent,
                      nsIASN1Sequence **retSequence)
{
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();
  if (sequence == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  *retSequence = nsnull;
  nsString text;
  GetOIDText(&algID->algorithm, nssComponent, text);
  if (!algID->parameters.len || algID->parameters.data[0] == nsIASN1Object::ASN1_NULL) {
    sequence->SetDisplayValue(text.get());
    sequence->SetProcessObjects(PR_FALSE);
  } else {
    nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
    printableItem->SetDisplayValue(text.get());
    nsCOMPtr<nsISupportsArray>asn1Objects;
    sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
    asn1Objects->AppendElement(printableItem);
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpAlgID").get(),
                                        text);
    printableItem->SetDisplayName(text.get());
    printableItem = new nsNSSASN1PrintableItem();
    asn1Objects->AppendElement(printableItem);
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpParams").get(),
                                        text);
    printableItem->SetDisplayName(text.get()); 
    ProcessRawBytes(&algID->parameters,text);
    printableItem->SetDisplayValue(text.get());
  }
  *retSequence = sequence;
  NS_ADDREF(*retSequence);
  return NS_OK;
}

static nsresult
ProcessTime(PRTime dispTime, const PRUnichar *displayName, 
            nsIASN1Sequence *parentSequence)
{
  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  nsString text;
  nsString tempString;

  PRExplodedTime explodedTime;
  PR_ExplodeTime(dispTime, PR_LocalTimeParameters, &explodedTime);

  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                              &explodedTime, tempString);

  text.Append(tempString);
  text.Append(NS_LITERAL_STRING("\n("));

  PRExplodedTime explodedTimeGMT;
  PR_ExplodeTime(dispTime, PR_GMTParameters, &explodedTimeGMT);

  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                              &explodedTimeGMT, tempString);

  text.Append(tempString);
  text.Append(NS_LITERAL_STRING(" GMT)"));

  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  printableItem->SetDisplayValue(text.get());
  printableItem->SetDisplayName(displayName);
  nsCOMPtr<nsISupportsArray> asn1Objects;
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(printableItem);
  return NS_OK;
}

static nsresult
ProcessSubjectPublicKeyInfo(CERTSubjectPublicKeyInfo *spki, 
                            nsIASN1Sequence *parentSequence,
                            nsINSSComponent *nssComponent)
{
  nsCOMPtr<nsIASN1Sequence> spkiSequence = new nsNSSASN1Sequence();

  if (spkiSequence == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsString text;
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSPKI").get(),
                                      text);
  spkiSequence->SetDisplayName(text.get());

  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSPKIAlg").get(),
                                      text);
  nsCOMPtr<nsIASN1Sequence> sequenceItem;
  nsresult rv = ProcessSECAlgorithmID(&spki->algorithm, nssComponent,
                                      getter_AddRefs(sequenceItem));
  if (NS_FAILED(rv))
    return rv;
  sequenceItem->SetDisplayName(text.get());
  nsCOMPtr<nsISupportsArray> asn1Objects;
  spkiSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(sequenceItem);

  // The subjectPublicKey field is encoded as a bit string.
  // ProcessRawBytes expects the lenght to be in bytes, so 
  // let's convert the lenght into a temporary SECItem.
  SECItem data;
  data.data = spki->subjectPublicKey.data;
  data.len  = spki->subjectPublicKey.len / 8;
  text.Truncate();
  ProcessRawBytes(&data, text);
  nsCOMPtr<nsIASN1PrintableItem> printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  printableItem->SetDisplayValue(text.get());
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSubjPubKey").get(),
                                      text);
  printableItem->SetDisplayName(text.get());
  asn1Objects->AppendElement(printableItem);
  
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(spkiSequence);
  return NS_OK;
}

static nsresult
ProcessExtensions(CERTCertExtension **extensions, 
                  nsIASN1Sequence *parentSequence, 
                  nsINSSComponent *nssComponent)
{
  nsCOMPtr<nsIASN1Sequence> extensionSequence = new nsNSSASN1Sequence;
  if (extensionSequence == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsString text;
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpExtensions").get(),
                                      text);
  extensionSequence->SetDisplayName(text.get());
  PRInt32 i;
  nsresult rv;
  nsCOMPtr<nsIASN1PrintableItem> newExtension;
  nsCOMPtr<nsISupportsArray> asn1Objects;
  extensionSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  for (i=0; extensions[i] != nsnull; i++) {
    rv = ProcessSingleExtension(extensions[i], nssComponent,
                                getter_AddRefs(newExtension));
    if (NS_FAILED(rv))
      return rv;

    asn1Objects->AppendElement(newExtension);
  }
  parentSequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  asn1Objects->AppendElement(extensionSequence);
  return NS_OK;
}

static nsresult
ProcessName(CERTName *name, nsINSSComponent *nssComponent, PRUnichar **value)
{
  CERTRDN** rdns;
  CERTRDN** rdn;
  CERTAVA** avas;
  CERTAVA* ava;
  SECItem *decodeItem = nsnull;
  nsString finalString;

  rdns = name->rdns;

  nsString type;
  nsresult rv;
  const PRUnichar *params[2];
  nsString avavalue;
  nsXPIDLString temp;
  CERTRDN **lastRdn;
  lastRdn = rdns;


  /* find last RDN */
  lastRdn = rdns;
  while (*lastRdn) lastRdn++;
  // The above whille loop will put us at the last member
  // of the array which is a NULL pointer.  So let's back
  // up one spot so that we have the last non-NULL entry in 
  // the array in preparation for traversing the 
  // RDN's (Relative Distinguished Name) in reverse oder.
  lastRdn--;
   
  /*
   * Loop over name contents in _reverse_ RDN order appending to string
   * When building the Ascii string, NSS loops over these entries in 
   * reverse order, so I will as well.  The difference is that NSS
   * will always place them in a one line string separated by commas,
   * where I want each entry on a single line.  I can't just use a comma
   * as my delimitter because it is a valid character to have in the 
   * value portion of the AVA and could cause trouble when parsing.
   */
  for (rdn = lastRdn; rdn >= rdns; rdn--) {
    avas = (*rdn)->avas;
    while ((ava = *avas++) != 0) {
      rv = GetOIDText(&ava->type, nssComponent, type);
      if (NS_FAILED(rv))
        return rv;

      //This function returns a string in UTF8 format.
      decodeItem = CERT_DecodeAVAValue(&ava->value);
      if(!decodeItem) {
         return NS_ERROR_FAILURE;
      }
      avavalue.AssignWithConversion((char*)decodeItem->data, decodeItem->len);

      SECITEM_FreeItem(decodeItem, PR_TRUE);
      params[0] = type.get();
      params[1] = avavalue.get();
      nssComponent->PIPBundleFormatStringFromName(NS_LITERAL_STRING("AVATemplate").get(),
                                                  params, 2, 
                                                  getter_Copies(temp));
      finalString += temp + NS_LITERAL_STRING("\n");
    }
  }
  *value = ToNewUnicode(finalString);    
  return NS_OK;
}

nsresult
nsNSSCertificate::CreateTBSCertificateASN1Struct(nsIASN1Sequence **retSequence,
                                                 nsINSSComponent *nssComponent)
{
  //
  //   TBSCertificate  ::=  SEQUENCE  {
  //        version         [0]  EXPLICIT Version DEFAULT v1,
  //        serialNumber         CertificateSerialNumber,
  //        signature            AlgorithmIdentifier,
  //        issuer               Name,
  //        validity             Validity,
  //        subject              Name,
  //        subjectPublicKeyInfo SubjectPublicKeyInfo,
  //        issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                             -- If present, version shall be v2 or v3
  //        subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                             -- If present, version shall be v2 or v3
  //        extensions      [3]  EXPLICIT Extensions OPTIONAL
  //                            -- If present, version shall be v3
  //        }
  //
  // This is the ASN1 structure we should be dealing with at this point.
  // The code in this method will assert this is the structure we're dealing
  // and then add more user friendly text for that field.
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();
  if (sequence == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  nsString text;
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpCertificate").get(),
                                      text);
  sequence->SetDisplayName(text.get());
  nsCOMPtr<nsIASN1PrintableItem> printableItem;
  
  nsCOMPtr<nsISupportsArray> asn1Objects;
  sequence->GetASN1Objects(getter_AddRefs(asn1Objects));

  nsresult rv = ProcessVersion(&mCert->version, nssComponent,
                               getter_AddRefs(printableItem));
  if (NS_FAILED(rv))
    return rv;

  asn1Objects->AppendElement(printableItem);
  
  rv = ProcessSerialNumberDER(&mCert->serialNumber, nssComponent,
                              getter_AddRefs(printableItem));

  if (NS_FAILED(rv))
    return rv;
  asn1Objects->AppendElement(printableItem); 

  nsCOMPtr<nsIASN1Sequence> algID;
  rv = ProcessSECAlgorithmID(&mCert->signature,
                             nssComponent, getter_AddRefs(algID));
  if (NS_FAILED(rv))
    return rv;

  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSigAlg").get(),
                                      text);
  algID->SetDisplayName(text.get());
  asn1Objects->AppendElement(algID);

  nsXPIDLString value;
  ProcessName(&mCert->issuer, nssComponent, getter_Copies(value));

  printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  printableItem->SetDisplayValue(value);
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpIssuer").get(),
                                      text);
  printableItem->SetDisplayName(text.get());
  asn1Objects->AppendElement(printableItem);
  
  nsCOMPtr<nsIASN1Sequence> validitySequence = new nsNSSASN1Sequence();
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpValidity").get(),
                                      text);
  validitySequence->SetDisplayName(text.get());
  asn1Objects->AppendElement(validitySequence);
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpNotBefore").get(),
                                      text);
  nsCOMPtr<nsIX509CertValidity> validityData;
  GetValidity(getter_AddRefs(validityData));
  PRTime notBefore, notAfter;

  validityData->GetNotBefore(&notBefore);
  validityData->GetNotAfter(&notAfter);
  validityData = 0;
  rv = ProcessTime(notBefore, text.get(), validitySequence);
  if (NS_FAILED(rv))
    return rv;

  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpNotAfter").get(),
                                      text);
  rv = ProcessTime(notAfter, text.get(), validitySequence);
  if (NS_FAILED(rv))
    return rv;

  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSubject").get(),
                                      text);

  printableItem = new nsNSSASN1PrintableItem();
  if (printableItem == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  printableItem->SetDisplayName(text.get());
  ProcessName(&mCert->subject, nssComponent,getter_Copies(value));
  printableItem->SetDisplayValue(value);
  asn1Objects->AppendElement(printableItem);

  rv = ProcessSubjectPublicKeyInfo(&mCert->subjectPublicKeyInfo, sequence,
                                   nssComponent); 
  if (NS_FAILED(rv))
    return rv;
 
  SECItem data; 
  // Is there an issuerUniqueID?
  if (mCert->issuerID.data != nsnull) {
    // The issuerID is encoded as a bit string.
    // The function ProcessRawBytes expects the
    // length to be in bytes, so let's convert the
    // length in a temporary SECItem
    data.data = mCert->issuerID.data;
    data.len  = mCert->issuerID.len / 8;

    ProcessRawBytes(&data, text);
    printableItem = new nsNSSASN1PrintableItem();
    if (printableItem == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

    printableItem->SetDisplayValue(text.get());
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpIssuerUniqueID").get(),
                                         text);
    printableItem->SetDisplayName(text.get());
    asn1Objects->AppendElement(printableItem);
  }

  if (mCert->subjectID.data) {
    // The subjectID is encoded as a bit string.
    // The function ProcessRawBytes expects the
    // length to be in bytes, so let's convert the
    // length in a temporary SECItem
    data.data = mCert->issuerID.data;
    data.len  = mCert->issuerID.len / 8;

    ProcessRawBytes(&data, text);
    printableItem = new nsNSSASN1PrintableItem();
    if (printableItem == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

    printableItem->SetDisplayValue(text.get());
    nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSubjectUniqueID").get(),
                                         text);
    printableItem->SetDisplayName(text.get());
    asn1Objects->AppendElement(printableItem);

  }
  if (mCert->extensions) {
    rv = ProcessExtensions(mCert->extensions, sequence, nssComponent);
    if (NS_FAILED(rv))
      return rv;
  }
  *retSequence = sequence;
  NS_ADDREF(*retSequence);  
  return NS_OK;
}

nsresult
nsNSSCertificate::CreateASN1Struct()
{
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence();

  mASN1Structure = sequence; 
  if (mASN1Structure == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsISupportsArray> asn1Objects;
  sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
  nsXPIDLCString title;
  GetWindowTitle(getter_Copies(title));
  
  mASN1Structure->SetDisplayName(NS_ConvertASCIItoUCS2(title).get());
  // This sequence will be contain the tbsCertificate, signatureAlgorithm,
  // and signatureValue.
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  rv = CreateTBSCertificateASN1Struct(getter_AddRefs(sequence),
                                      nssComponent);
  if (NS_FAILED(rv))
    return rv;

  asn1Objects->AppendElement(sequence);
  nsCOMPtr<nsIASN1Sequence> algID;

  rv = ProcessSECAlgorithmID(&mCert->signatureWrap.signatureAlgorithm, 
                             nssComponent, getter_AddRefs(algID));
  if (NS_FAILED(rv))
    return rv;
  nsString text;
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpSigAlg").get(),
                                      text);
  algID->SetDisplayName(text.get());
  asn1Objects->AppendElement(algID);
  nsCOMPtr<nsIASN1PrintableItem>printableItem = new nsNSSASN1PrintableItem();
  nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CertDumpCertSig").get(),
                                      text);
  printableItem->SetDisplayName(text.get());
  // The signatureWrap is encoded as a bit string.
  // The function ProcessRawBytes expects the
  // length to be in bytes, so let's convert the
  // length in a temporary SECItem
  SECItem temp;
  temp.data = mCert->signatureWrap.signature.data;
  temp.len  = mCert->signatureWrap.signature.len / 8;
  text.Truncate();
  ProcessRawBytes(&temp,text);
  printableItem->SetDisplayValue(text.get());
  asn1Objects->AppendElement(printableItem);
  return NS_OK;
}

/* readonly attribute nsIASN1Object ASN1Structure; */
NS_IMETHODIMP 
nsNSSCertificate::GetASN1Structure(nsIASN1Object * *aASN1Structure)
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG_POINTER(aASN1Structure);
  if (mASN1Structure == nsnull) {
    // First create the recursive structure os ASN1Objects
    // which tells us the layout of the cert.
    rv = CreateASN1Struct();
    if (NS_FAILED(rv)) {
      return rv;
    }
#ifdef DEBUG_javi
    DumpASN1Object(mASN1Structure, 0);
#endif
  }
  *aASN1Structure = mASN1Structure;
  NS_IF_ADDREF(*aASN1Structure);
  return rv;
}

NS_IMETHODIMP
nsNSSCertificate::IsSameCert(nsIX509Cert *other, PRBool *result)
{
  NS_ENSURE_ARG(other);
  NS_ENSURE_ARG(result);

  nsNSSCertificate *other2 = NS_STATIC_CAST(nsNSSCertificate*, other);
  if (!other2)
    return NS_ERROR_FAILURE;
  
  *result = (mCert == other2->mCert);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::SaveSMimeProfile()
{
  if (SECSuccess != CERT_SaveSMimeProfile(mCert, nsnull, nsnull))
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}


/* nsNSSCertificateDB */

NS_IMPL_ISUPPORTS1(nsNSSCertificateDB, nsIX509CertDB)

nsNSSCertificateDB::nsNSSCertificateDB()
{
  NS_INIT_ISUPPORTS();
}

nsNSSCertificateDB::~nsNSSCertificateDB()
{
}

/*  nsIX509Cert getCertByNickname(in nsIPK11Token aToken,
 *                                in wstring aNickname);
 */
NS_IMETHODIMP
nsNSSCertificateDB::GetCertByNickname(nsIPK11Token *aToken,
                                      const PRUnichar *nickname,
                                      nsIX509Cert **_rvCert)
{
  CERTCertificate *cert = NULL;
  char *asciiname = NULL;
  NS_ConvertUCS2toUTF8 aUtf8Nickname(nickname);
  asciiname = NS_CONST_CAST(char*, aUtf8Nickname.get());
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting \"%s\"\n", asciiname));
#if 0
  // what it should be, but for now...
  if (aToken) {
    cert = PK11_FindCertFromNickname(asciiname, NULL);
  } else {
    cert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), asciiname);
  }
#endif
  cert = PK11_FindCertFromNickname(asciiname, NULL);
  if (!cert) {
    cert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), asciiname);
  }
  if (cert) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("got it\n"));
    nsCOMPtr<nsIX509Cert> pCert = new nsNSSCertificate(cert);
    *_rvCert = pCert;
    NS_ADDREF(*_rvCert);
    return NS_OK;
  }
  *_rvCert = nsnull;
  return NS_ERROR_FAILURE;
}

/* nsIX509Cert getCertByDBKey(in string aDBkey, in nsIPK11Token aToken); */
NS_IMETHODIMP 
nsNSSCertificateDB::GetCertByDBKey(const char *aDBkey, nsIPK11Token *aToken,
                                   nsIX509Cert **_cert)
{
  SECItem keyItem = {siBuffer, nsnull, 0};
  SECItem *dummy;
  CERTIssuerAndSN issuerSN;
  unsigned long moduleID,slotID;
  *_cert = nsnull; 
  if (!aDBkey) return NS_ERROR_FAILURE;
  dummy = NSSBase64_DecodeBuffer(nsnull, &keyItem, aDBkey,
                                 (PRUint32)PL_strlen(aDBkey)); 
  CERTCertificate *cert;

  // someday maybe we can speed up the search using the moduleID and slotID
  moduleID = NS_NSS_GET_LONG(keyItem.data);
  slotID = NS_NSS_GET_LONG(&keyItem.data[NS_NSS_LONG]);

  // build the issuer/SN structure
  issuerSN.serialNumber.len = NS_NSS_GET_LONG(&keyItem.data[NS_NSS_LONG*2]);
  issuerSN.derIssuer.len = NS_NSS_GET_LONG(&keyItem.data[NS_NSS_LONG*3]);
  issuerSN.serialNumber.data= &keyItem.data[NS_NSS_LONG*4];
  issuerSN.derIssuer.data= &keyItem.data[NS_NSS_LONG*4+
                                              issuerSN.serialNumber.len];

  cert = CERT_FindCertByIssuerAndSN(CERT_GetDefaultCertDB(), &issuerSN);
  PR_FREEIF(keyItem.data);
  if (cert) {
    nsNSSCertificate *nssCert = new nsNSSCertificate(cert);
    if (nssCert == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(nssCert);
    *_cert = NS_STATIC_CAST(nsIX509Cert*, nssCert);
  }
  return NS_OK;
}

/*
 * void getCertNicknames(in nsIPK11Token aToken, 
 *                       in unsigned long aType,
 *                       out unsigned long count,
 *                       [array, size_is(count)] out wstring certNameList);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::GetCertNicknames(nsIPK11Token *aToken, 
                                     PRUint32      aType,
                                     PRUint32     *_count,
                                     PRUnichar  ***_certNames)
{
  nsresult rv = NS_ERROR_FAILURE;
  /*
   * obtain the cert list from NSS
   */
  CERTCertList *certList = NULL;
  PK11CertListType pk11type;
#if 0
  // this would seem right, but it didn't work...
  // oh, I know why - bonks out on internal slot certs
  if (aType == nsIX509Cert::USER_CERT)
    pk11type = PK11CertListUser;
  else 
#endif
    pk11type = PK11CertListUnique;
  certList = PK11_ListCerts(pk11type, NULL);
  if (!certList)
    goto cleanup;
  /*
   * get list of cert names from list of certs
   * XXX also cull the list (NSS only distinguishes based on user/non-user
   */
  getCertNames(certList, aType, _count, _certNames);
  rv = NS_OK;
  /*
   * finish up
   */
cleanup:
  if (certList)
    CERT_DestroyCertList(certList);
  return rv;
}

PRBool
nsNSSCertificateDB::GetCertsByTypeFromCertList(CERTCertList *aCertList,
                                               PRUint32 aType,
                                               nsCertCompareFunc  aCertCmpFn,
                                               void *aCertCmpFnArg,
                                               nsISupportsArray **_certs)
{
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("GetCertsByTypeFromCertList"));
  if (!aCertList)
    return PR_FALSE;
  nsCOMPtr<nsISupportsArray> certarray;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(certarray));
  if (NS_FAILED(rv)) return PR_FALSE;
  CERTCertListNode *node;
  int i, count = 0;
  for (node = CERT_LIST_HEAD(aCertList);
       !CERT_LIST_END(node, aCertList);
       node = CERT_LIST_NEXT(node)) {
    if (getCertType(node->cert) == aType) {
      nsCOMPtr<nsIX509Cert> pipCert = new nsNSSCertificate(node->cert);
      if (pipCert) {
        for (i=0; i<count; i++) {
          nsCOMPtr<nsISupports> isupport = 
            dont_AddRef(certarray->ElementAt(i));
          nsCOMPtr<nsIX509Cert> cert = do_QueryInterface(isupport);
          if ((*aCertCmpFn)(aCertCmpFnArg, pipCert, cert) < 0) {
            certarray->InsertElementAt(pipCert, i);
            break;
          }
        }
        if (i == count) certarray->AppendElement(pipCert);
          count++;
      }
    }
  }
  *_certs = certarray;
  NS_ADDREF(*_certs);
  return PR_TRUE;
}

PRBool 
nsNSSCertificateDB::GetCertsByType(PRUint32           aType,
                                   nsCertCompareFunc  aCertCmpFn,
                                   void              *aCertCmpFnArg,
                                   nsISupportsArray **_certs)
{
  CERTCertList *certList = NULL;
  nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();
  certList = PK11_ListCerts(PK11CertListUnique, cxt);
  PRBool rv = GetCertsByTypeFromCertList(certList, aType, aCertCmpFn, aCertCmpFnArg, _certs);
  if (certList)
    CERT_DestroyCertList(certList);
  return rv;
}

PRBool 
nsNSSCertificateDB::GetCertsByTypeFromCache(nsINSSCertCache   *aCache,
                                            PRUint32           aType,
                                            nsCertCompareFunc  aCertCmpFn,
                                            void              *aCertCmpFnArg,
                                            nsISupportsArray **_certs)
{
  NS_ENSURE_ARG_POINTER(aCache);
  CERTCertList *certList = NS_REINTERPRET_CAST(CERTCertList*, aCache->GetCachedCerts());
  if (!certList)
    return NS_ERROR_FAILURE;
  return GetCertsByTypeFromCertList(certList, aType, aCertCmpFn, aCertCmpFnArg, _certs);
}


SECStatus PR_CALLBACK
collect_certs(void *arg, SECItem **certs, int numcerts)
{
  CERTDERCerts *collectArgs;
  SECItem *cert;
  SECStatus rv;

  collectArgs = (CERTDERCerts *)arg;

  collectArgs->numcerts = numcerts;
  collectArgs->rawCerts = (SECItem *) PORT_ArenaZAlloc(collectArgs->arena,
                                           sizeof(SECItem) * numcerts);
  if ( collectArgs->rawCerts == NULL )
    return(SECFailure);

  cert = collectArgs->rawCerts;

  while ( numcerts-- ) {
    rv = SECITEM_CopyItem(collectArgs->arena, cert, *certs);
    if ( rv == SECFailure )
      return(SECFailure);
    cert++;
    certs++;
  }

  return (SECSuccess);
}

CERTDERCerts*
nsNSSCertificateDB::getCertsFromPackage(PRArenaPool *arena, char *data, 
                                        PRUint32 length)
{
  CERTDERCerts *collectArgs = 
               (CERTDERCerts *)PORT_ArenaZAlloc(arena, sizeof(CERTDERCerts));
  if ( collectArgs == nsnull ) 
    return nsnull;

  collectArgs->arena = arena;
  SECStatus sec_rv = CERT_DecodeCertPackage(data, length, collect_certs, 
                                            (void *)collectArgs);
  if (sec_rv != SECSuccess)
    return nsnull;

  return collectArgs;
}

nsresult
nsNSSCertificateDB::handleCACertDownload(nsISupportsArray *x509Certs,
                                         nsIInterfaceRequestor *ctx)
{
  // First thing we have to do is figure out which certificate we're 
  // gonna present to the user.  The CA may have sent down a list of 
  // certs which may or may not be a chained list of certs.  Until
  // the day we can design some solid UI for the general case, we'll
  // code to the > 90% case.  That case is where a CA sends down a
  // list that is a chain up to its root in either ascending or 
  // descending order.  What we're gonna do is compare the first 
  // 2 entries, if the first was signed by the second, we assume
  // the leaf cert is the first cert and display it.  If the second
  // cert was signed by the first cert, then we assume the first cert
  // is the root and the last cert in the array is the leaf.  In this
  // case we display the last cert.
  PRUint32 numCerts;

  x509Certs->Count(&numCerts);
  NS_ASSERTION(numCerts > 0, "Didn't get any certs to import.");
  if (numCerts == 0)
    return NS_OK; // Nothing to import, so nothing to do.

  nsCOMPtr<nsIX509Cert> certToShow;
  nsCOMPtr<nsISupports> isupports;
  PRUint32 selCertIndex;
  if (numCerts == 1) {
    // There's only one cert, so let's show it.
    selCertIndex = 0;
    isupports = dont_AddRef(x509Certs->ElementAt(selCertIndex));
    certToShow = do_QueryInterface(isupports);
  } else {
    nsCOMPtr<nsIX509Cert> cert0;
    nsCOMPtr<nsIX509Cert> cert1;

    isupports = dont_AddRef(x509Certs->ElementAt(0));
    cert0 = do_QueryInterface(isupports);

    isupports = dont_AddRef(x509Certs->ElementAt(1));
    cert1 = do_QueryInterface(isupports);

    nsXPIDLString cert0SubjectName;
    nsXPIDLString cert0IssuerName;
    nsXPIDLString cert1SubjectName;
    nsXPIDLString cert1IssuerName;

    cert0->GetIssuerName(getter_Copies(cert0IssuerName));
    cert0->GetSubjectName(getter_Copies(cert0SubjectName));

    cert1->GetIssuerName(getter_Copies(cert1IssuerName));
    cert1->GetSubjectName(getter_Copies(cert1SubjectName));

    if (nsCRT::strcmp(cert1IssuerName.get(), cert0SubjectName.get()) == 0) {
      // In this case, the first cert in the list signed the second,
      // so the first cert is the root.  Let's display the last cert 
      // in the list.
      selCertIndex = numCerts-1;
      isupports = dont_AddRef(x509Certs->ElementAt(selCertIndex));
      certToShow = do_QueryInterface(isupports);
    } else 
    if (nsCRT::strcmp(cert0IssuerName.get(), cert1SubjectName.get()) == 0) { 
      // In this case the second cert has signed the first cert.  The 
      // first cert is the leaf, so let's display it.
      selCertIndex = 0;
      certToShow = cert0;
    } else {
      // It's not a chain, so let's just show the first one in the 
      // downloaded list.
      selCertIndex = 0;
      certToShow = cert0;
    }
  }

  if (!certToShow)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsICertificateDialogs> dialogs;
  nsresult rv = ::getNSSDialogs(getter_AddRefs(dialogs), 
                                NS_GET_IID(nsICertificateDialogs));
                       
  if (NS_FAILED(rv))
    return rv;
 
  SECItem der;
  rv=certToShow->GetRawDER((char **)&der.data, &der.len);

  if (NS_FAILED(rv))
    return rv;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Creating temp cert\n"));
  CERTCertificate *tmpCert;
  CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
  tmpCert = CERT_FindCertByDERCert(certdb, &der);
  if (!tmpCert) {
    tmpCert = CERT_NewTempCertificate(certdb, &der,
                                      nsnull, PR_FALSE, PR_TRUE);
  }
  if (!tmpCert) {
    NS_ASSERTION(0,"Couldn't create cert from DER blob\n");
    return NS_ERROR_FAILURE;
  }

  CERTCertificateCleaner tmpCertCleaner(tmpCert);

  PRBool canceled;
  if (tmpCert->isperm) {
    dialogs->CACertExists(ctx, &canceled);
    return NS_ERROR_FAILURE;
  }

  PRUint32 trustBits;
  rv = dialogs->DownloadCACert(ctx, certToShow, &trustBits, &canceled);
  if (NS_FAILED(rv))
    return rv;

  if (canceled)
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("trust is %d\n", trustBits));
  nsXPIDLCString nickname;
  nickname.Adopt(CERT_MakeCANickname(tmpCert));

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Created nick \"%s\"\n", nickname.get()));

  nsNSSCertTrust trust;
  trust.SetValidCA();
  trust.AddCATrust(trustBits & nsIX509CertDB::TRUSTED_SSL,
                   trustBits & nsIX509CertDB::TRUSTED_EMAIL,
                   trustBits & nsIX509CertDB::TRUSTED_OBJSIGN);

  SECStatus srv = CERT_AddTempCertToPerm(tmpCert, 
                                         NS_CONST_CAST(char*,nickname.get()), 
                                         trust.GetTrust()); 

  if (srv != SECSuccess)
    return NS_ERROR_FAILURE;

  // Now it's time to add the rest of the certs we just downloaded.
  // Since we didn't prompt the user about any of these certs, we
  // won't set any trust bits for them.
  nsNSSCertTrust defaultTrust;
  defaultTrust.SetValidCA();
  defaultTrust.AddCATrust(0,0,0);
  for (PRUint32 i=0; i<numCerts; i++) {
    if (i == selCertIndex)
      continue;

    isupports = dont_AddRef(x509Certs->ElementAt(i));
    certToShow = do_QueryInterface(isupports);
    certToShow->GetRawDER((char **)&der.data, &der.len);

    CERTCertificate *tmpCert2 = 
      CERT_NewTempCertificate(certdb, &der, nsnull, PR_FALSE, PR_TRUE);

    if (!tmpCert2) {
      NS_ASSERTION(0, "Couldn't create temp cert from DER blob\n");
      continue;  // Let's try to import the rest of 'em
    }
    nickname.Adopt(CERT_MakeCANickname(tmpCert2));
    CERT_AddTempCertToPerm(tmpCert2, NS_CONST_CAST(char*,nickname.get()), 
                           defaultTrust.GetTrust());
    CERT_DestroyCertificate(tmpCert2);
  }
  
  return NS_OK;  
}

/*
 *  [noscript] void importCertificates(in charPtr data, in unsigned long length,
 *                                     in unsigned long type, 
 *                                     in nsIInterfaceRequestor ctx);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::ImportCertificates(char * data, PRUint32 length, 
                                       PRUint32 type, 
                                       nsIInterfaceRequestor *ctx)

{
  nsresult nsrv;

  PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_OUT_OF_MEMORY;

  CERTDERCerts *certCollection = getCertsFromPackage(arena, data, length);
  if (!certCollection) {
    PORT_FreeArena(arena, PR_FALSE);
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISupportsArray> array;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) {
    PORT_FreeArena(arena, PR_FALSE);
    return rv;
  }

  // Now let's create some certs to work with
  nsCOMPtr<nsIX509Cert> x509Cert;
  nsNSSCertificate *nssCert;
  SECItem *currItem;
  for (int i=0; i<certCollection->numcerts; i++) {
     currItem = &certCollection->rawCerts[i];
     nssCert = nsNSSCertificate::ConstructFromDER((char*)currItem->data, currItem->len);
     if (!nssCert)
       return NS_ERROR_FAILURE;
     x509Cert = do_QueryInterface(nssCert);
     array->AppendElement(x509Cert);
  }
  switch (type) {
  case nsIX509Cert::CA_CERT:
    nsrv = handleCACertDownload(array, ctx);
    break;
  default:
    // We only deal with import CA certs in this method currently.
     nsrv = NS_ERROR_FAILURE;
     break;
  }  
  PORT_FreeArena(arena, PR_FALSE);
  return nsrv;
}


NS_IMETHODIMP
nsNSSCertificateDB::ImportEmailCertificate2(PRUint32 length, PRUint8 *data)
{
  return ImportEmailCertificate(NS_REINTERPRET_CAST(char*, data), length, nsnull);
}

/*
 *  [noscript] void importEmailCertificates(in charPtr data, in unsigned long length,
 *                                     in nsIInterfaceRequestor ctx);
 */
NS_IMETHODIMP
nsNSSCertificateDB::ImportEmailCertificate(char * data, PRUint32 length, 
                                       nsIInterfaceRequestor *ctx)

{
  SECStatus srv = SECFailure;
  nsresult nsrv = NS_OK;
  CERTCertificate * cert;
  SECItem **rawCerts;
  int numcerts;
  int i;
 
  PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_OUT_OF_MEMORY;

  CERTDERCerts *certCollection = getCertsFromPackage(arena, data, length);
  if (!certCollection) {
    PORT_FreeArena(arena, PR_FALSE);
    return NS_ERROR_FAILURE;
  }
  cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), certCollection->rawCerts,
                          (char *)NULL, PR_FALSE, PR_TRUE);
  if (!cert) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }
  numcerts = certCollection->numcerts;
  rawCerts = (SECItem **) PORT_Alloc(sizeof(SECItem *) * numcerts);
  if ( !rawCerts ) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }

  for ( i = 0; i < numcerts; i++ ) {
    rawCerts[i] = &certCollection->rawCerts[i];
  }
 
  srv = CERT_ImportCerts(CERT_GetDefaultCertDB(), certUsageEmailSigner,
             numcerts, rawCerts, NULL, PR_TRUE, PR_FALSE,
             NULL);
  if ( srv != SECSuccess ) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }
  srv = CERT_SaveSMimeProfile(cert, NULL, NULL);
  PORT_Free(rawCerts);
loser:
  if (arena) 
    PORT_FreeArena(arena, PR_TRUE);
  return nsrv;
}

char* nsNSSCertificate::defaultServerNickname(CERTCertificate* cert)
{
  char* nickname = nsnull;
  int count;
  PRBool conflict;
  char* servername = nsnull;
  
  servername = CERT_GetCommonName(&cert->subject);
  if (servername == NULL) {
    return nsnull;
  }
   
  count = 1;
  while (1) {
    if (count == 1) {
      nickname = PR_smprintf("%s", servername);
    }
    else {
      nickname = PR_smprintf("%s #%d", servername, count);
    }
    if (nickname == NULL) {
      break;
    }

    conflict = SEC_CertNicknameConflict(nickname, &cert->derSubject,
                                        cert->dbhandle);
    if (conflict == PR_SUCCESS) {
      break;
    }
    PR_Free(nickname);
    count++;
  }
  PR_FREEIF(servername);
  return nickname;
}


NS_IMETHODIMP
nsNSSCertificateDB::ImportServerCertificate(char * data, PRUint32 length, 
                                            nsIInterfaceRequestor *ctx)

{
  SECStatus srv = SECFailure;
  nsresult nsrv = NS_OK;
  CERTCertificate * cert;
  SECItem **rawCerts = nsnull;
  int numcerts;
  int i;
  nsNSSCertTrust trust;
  char *serverNickname = nsnull;
 
  PRArenaPool *arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena)
    return NS_ERROR_OUT_OF_MEMORY;

  CERTDERCerts *certCollection = getCertsFromPackage(arena, data, length);
  if (!certCollection) {
    PORT_FreeArena(arena, PR_FALSE);
    return NS_ERROR_FAILURE;
  }
  cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), certCollection->rawCerts,
                          (char *)NULL, PR_FALSE, PR_TRUE);
  if (!cert) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }
  numcerts = certCollection->numcerts;
  rawCerts = (SECItem **) PORT_Alloc(sizeof(SECItem *) * numcerts);
  if ( !rawCerts ) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }

  for ( i = 0; i < numcerts; i++ ) {
    rawCerts[i] = &certCollection->rawCerts[i];
  }

  serverNickname = nsNSSCertificate::defaultServerNickname(cert);
  srv = CERT_ImportCerts(CERT_GetDefaultCertDB(), certUsageSSLServer,
             numcerts, rawCerts, NULL, PR_TRUE, PR_FALSE,
             serverNickname);
  PR_FREEIF(serverNickname);
  if ( srv != SECSuccess ) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }

  trust.SetValidServerPeer();
  srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), cert, trust.GetTrust());
  if ( srv != SECSuccess ) {
    nsrv = NS_ERROR_FAILURE;
    goto loser;
  }
loser:
  PORT_Free(rawCerts);
  if (arena) 
    PORT_FreeArena(arena, PR_TRUE);
  return nsrv;
}


char *
default_nickname(CERTCertificate *cert, nsIInterfaceRequestor* ctx)
{   
  nsresult rv;
  char *username = NULL;
  char *caname = NULL;
  char *nickname = NULL;
  char *tmp = NULL;
  int count;
  char *nickFmt=NULL, *nickFmtWithNum = NULL;
  CERTCertificate *dummycert;
  PK11SlotInfo *slot=NULL;
  CK_OBJECT_HANDLE keyHandle;
  nsAutoString tmpNickFmt;
  nsAutoString tmpNickFmtWithNum;

  CERTCertDBHandle *defaultcertdb = CERT_GetDefaultCertDB();
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv)) goto loser; 

  username = CERT_GetCommonName(&cert->subject);
  if ( username == NULL ) 
    username = PL_strdup("");

  if ( username == NULL ) 
    goto loser;
    
  caname = CERT_GetOrgName(&cert->issuer);
  if ( caname == NULL ) 
    caname = PL_strdup("");
  
  if ( caname == NULL ) 
    goto loser;
  
  count = 1;
  nssComponent->GetPIPNSSBundleString(
                              NS_LITERAL_STRING("nick_template").get(),
                              tmpNickFmt);
  nickFmt = ToNewUTF8String(tmpNickFmt);

  nssComponent->GetPIPNSSBundleString(
                              NS_LITERAL_STRING("nick_template_with_num").get(),
                              tmpNickFmtWithNum);
  nickFmtWithNum = ToNewUTF8String(tmpNickFmtWithNum);


  nickname = PR_smprintf(nickFmt, username, caname);
  /*
   * We need to see if the private key exists on a token, if it does
   * then we need to check for nicknames that already exist on the smart
   * card.
   */
  slot = PK11_KeyForCertExists(cert, &keyHandle, ctx);
  if (slot == NULL) {
    goto loser;
  }
  if (!PK11_IsInternal(slot)) {
    tmp = PR_smprintf("%s:%s", PK11_GetTokenName(slot), nickname);
    PR_Free(nickname);
    nickname = tmp;
    tmp = NULL;
  }
  tmp = nickname;
  while ( 1 ) {	
    if ( count > 1 ) {
      nickname = PR_smprintf("%s #%d", tmp, count);
    }
  
    if ( nickname == NULL ) 
      goto loser;
 
    if (PK11_IsInternal(slot)) {
      /* look up the nickname to make sure it isn't in use already */
      dummycert = CERT_FindCertByNickname(defaultcertdb, nickname);
      
    } else {
      /*
       * Check the cert against others that already live on the smart 
       * card.
       */
      dummycert = PK11_FindCertFromNickname(nickname, ctx);
      if (dummycert != NULL) {
	/*
	 * Make sure the subject names are different.
	 */ 
	if (CERT_CompareName(&cert->subject, &dummycert->subject) == SECEqual)
	{
	  /*
	   * There is another certificate with the same nickname and
	   * the same subject name on the smart card, so let's use this
	   * nickname.
	   */
	  CERT_DestroyCertificate(dummycert);
	  dummycert = NULL;
	}
      }
    }
    if ( dummycert == NULL ) 
      goto done;
    
    /* found a cert, destroy it and loop */
    CERT_DestroyCertificate(dummycert);
    if (tmp != nickname) PR_Free(nickname);
    count++;
  } /* end of while(1) */
    
loser:
  if ( nickname ) {
    PR_Free(nickname);
  }
  nickname = NULL;
done:
  if ( caname ) {
    PR_Free(caname);
  }
  if ( username )  {
    PR_Free(username);
  }
  if (slot != NULL) {
      PK11_FreeSlot(slot);
      if (nickname != NULL) {
	      tmp = nickname;
	      nickname = strchr(tmp, ':');
	      if (nickname != NULL) {
	        nickname++;
	        nickname = PL_strdup(nickname);
	        PR_Free(tmp);
             tmp = nsnull;
	      } else {
	        nickname = tmp;
	        tmp = NULL;
	      }
      }
    }
    PR_FREEIF(tmp);
    return(nickname);
}


NS_IMETHODIMP 
nsNSSCertificateDB::ImportUserCertificate(char *data, PRUint32 length, nsIInterfaceRequestor *ctx)
{
  PK11SlotInfo *slot;
  char * nickname = NULL;
  nsresult rv = NS_ERROR_FAILURE;
  int numCACerts;
  SECItem *CACerts;
  CERTDERCerts * collectArgs;
  PRArenaPool *arena;
  CERTCertificate * cert=NULL;

  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if ( arena == NULL ) {
    goto loser;
  }

  collectArgs = getCertsFromPackage(arena, data, length);
  if (!collectArgs) {
    goto loser;
  }

  cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), collectArgs->rawCerts,
                	       (char *)NULL, PR_FALSE, PR_TRUE);
  if (!cert) {
    goto loser;
  }

  slot = PK11_KeyForCertExists(cert, NULL, ctx);
  if ( slot == NULL ) {
    goto loser;
  }
  PK11_FreeSlot(slot);

  /* pick a nickname for the cert */
  if (cert->nickname) {
	/* sigh, we need a call to look up other certs with this subject and
	 * identify nicknames from them. We can no longer walk down internal
	 * database structures  rjr */
  	nickname = cert->nickname;
  }
  else {
    nickname = default_nickname(cert, ctx);
  }

  /* user wants to import the cert */
  slot = PK11_ImportCertForKey(cert, nickname, ctx);
  if (!slot) {
    goto loser;
  }
  PK11_FreeSlot(slot);
  numCACerts = collectArgs->numcerts - 1;

  if (numCACerts) {
    CACerts = collectArgs->rawCerts+1;
    if ( ! CERT_ImportCAChain(CACerts, numCACerts, certUsageUserCertImport) ) {
      rv = NS_OK;
    }
  }
  
loser:
  if (arena) {
    PORT_FreeArena(arena, PR_FALSE);
  }
  if ( cert ) {
    CERT_DestroyCertificate(cert);
  }
  return rv;
}

/*
 * void deleteCertificate(in nsIX509Cert aCert);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::DeleteCertificate(nsIX509Cert *aCert)
{
  nsNSSCertificate *nssCert = NS_STATIC_CAST(nsNSSCertificate*, aCert);
  CERTCertificate *cert = nssCert->GetCert();
  if (!cert) return NS_ERROR_FAILURE;
  SECStatus srv = SECSuccess;

  PRUint32 certType = getCertType(cert);
  nssCert->SetCertType(certType);
  if (NS_FAILED(nssCert->MarkForPermDeletion()))
  {
    return NS_ERROR_FAILURE;
  }

  if (cert->slot && certType != nsIX509Cert::USER_CERT) {
    // To delete a cert of a slot (builtin, most likely), mark it as
    // completely untrusted.  This way we keep a copy cached in the
    // local database, and next time we try to load it off of the 
    // external token/slot, we'll know not to trust it.  We don't 
    // want to do that with user certs, because a user may  re-store
    // the cert onto the card again at which point we *will* want to 
    // trust that cert if it chains up properly.
    nsNSSCertTrust trust(0, 0, 0);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
                               cert, trust.GetTrust());
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("cert deleted: %d", srv));
  return (srv) ? NS_ERROR_FAILURE : NS_OK;
}

/*
 * void setCertTrust(in nsIX509Cert cert,
 *                   in unsigned long type,
 *                   in unsigned long trust);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::SetCertTrust(nsIX509Cert *cert, 
                                 PRUint32 type,
                                 PRUint32 trusted)
{
  SECStatus srv;
  nsNSSCertTrust trust;
  nsNSSCertificate *pipCert = NS_STATIC_CAST(nsNSSCertificate *, cert);
  CERTCertificate *nsscert = pipCert->GetCert();
  if (type == nsIX509Cert::CA_CERT) {
    // always start with untrusted and move up
    trust.SetValidCA();
    trust.AddCATrust(trusted & nsIX509CertDB::TRUSTED_SSL,
                     trusted & nsIX509CertDB::TRUSTED_EMAIL,
                     trusted & nsIX509CertDB::TRUSTED_OBJSIGN);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
                               nsscert,
                               trust.GetTrust());
  } else if (type == nsIX509Cert::SERVER_CERT) {
    // always start with untrusted and move up
    trust.SetValidPeer();
    trust.AddPeerTrust(trusted & nsIX509CertDB::TRUSTED_SSL, 0, 0);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
                               nsscert,
                               trust.GetTrust());
  } else if (type == nsIX509Cert::EMAIL_CERT) {
    // always start with untrusted and move up
    trust.SetValidPeer();
    trust.AddPeerTrust(0, trusted & nsIX509CertDB::TRUSTED_EMAIL, 0);
    srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
                               nsscert,
                               trust.GetTrust());
  } else {
    // ignore user certs
    return NS_OK;
  }
  return (srv) ? NS_ERROR_FAILURE : NS_OK;
}

/*
 * boolean getCertTrust(in nsIX509Cert cert,
 *                      in unsigned long certType,
 *                      in unsigned long trustType);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::GetCertTrust(nsIX509Cert *cert, 
                                 PRUint32 certType,
                                 PRUint32 trustType,
                                 PRBool *_isTrusted)
{
  SECStatus srv;
  nsNSSCertificate *pipCert = NS_STATIC_CAST(nsNSSCertificate *, cert);
  CERTCertificate *nsscert = pipCert->GetCert();
  CERTCertTrust nsstrust;
  srv = CERT_GetCertTrust(nsscert, &nsstrust);
  nsNSSCertTrust trust(&nsstrust);
  if (certType == nsIX509Cert::CA_CERT) {
    if (trustType & nsIX509CertDB::TRUSTED_SSL) {
      *_isTrusted = trust.HasTrustedCA(PR_TRUE, PR_FALSE, PR_FALSE);
    } else if (trustType & nsIX509CertDB::TRUSTED_EMAIL) {
      *_isTrusted = trust.HasTrustedCA(PR_FALSE, PR_TRUE, PR_FALSE);
    } else if (trustType & nsIX509CertDB::TRUSTED_OBJSIGN) {
      *_isTrusted = trust.HasTrustedCA(PR_FALSE, PR_FALSE, PR_TRUE);
    } else {
      return NS_ERROR_FAILURE;
    }
  } else if (certType == nsIX509Cert::SERVER_CERT) {
    if (trustType & nsIX509CertDB::TRUSTED_SSL) {
      *_isTrusted = trust.HasTrustedPeer(PR_TRUE, PR_FALSE, PR_FALSE);
    } else if (trustType & nsIX509CertDB::TRUSTED_EMAIL) {
      *_isTrusted = trust.HasTrustedPeer(PR_FALSE, PR_TRUE, PR_FALSE);
    } else if (trustType & nsIX509CertDB::TRUSTED_OBJSIGN) {
      *_isTrusted = trust.HasTrustedPeer(PR_FALSE, PR_FALSE, PR_TRUE);
    } else {
      return NS_ERROR_FAILURE;
    }
  } else if (certType == nsIX509Cert::EMAIL_CERT) {
    if (trustType & nsIX509CertDB::TRUSTED_SSL) {
      *_isTrusted = trust.HasTrustedPeer(PR_TRUE, PR_FALSE, PR_FALSE);
    } else if (trustType & nsIX509CertDB::TRUSTED_EMAIL) {
      *_isTrusted = trust.HasTrustedPeer(PR_FALSE, PR_TRUE, PR_FALSE);
    } else if (trustType & nsIX509CertDB::TRUSTED_OBJSIGN) {
      *_isTrusted = trust.HasTrustedPeer(PR_FALSE, PR_FALSE, PR_TRUE);
    } else {
      return NS_ERROR_FAILURE;
    }
  } /* user: ignore */
  return NS_OK;
}


NS_IMETHODIMP 
nsNSSCertificateDB::ImportCertsFromFile(nsIPK11Token *aToken, 
                                        nsILocalFile *aFile,
                                        PRUint32 aType)
{
  switch (aType) {
    case nsIX509Cert::CA_CERT:
    case nsIX509Cert::EMAIL_CERT:
    case nsIX509Cert::SERVER_CERT:
      // good
      break;
    
    default:
      // not supported (yet)
      return NS_ERROR_FAILURE;
  }

  nsresult rv;
  PRFileDesc *fd = nsnull;

  rv = aFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fd);

  if (NS_FAILED(rv))
    return rv;

  if (!fd)
    return NS_ERROR_FAILURE;

  PRFileInfo file_info;
  if (PR_SUCCESS != PR_GetOpenFileInfo(fd, &file_info))
    return NS_ERROR_FAILURE;
  
  char *buf = new char[file_info.size];
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRInt32 bytes_obtained = PR_Read(fd, buf, file_info.size);
  PR_Close(fd);
  
  if (bytes_obtained != file_info.size)
    rv = NS_ERROR_FAILURE;
  else {
	  nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();

    switch (aType) {
      case nsIX509Cert::CA_CERT:
        rv = ImportCertificates(buf, bytes_obtained, aType, cxt);
        break;
        
      case nsIX509Cert::SERVER_CERT:
        rv = ImportServerCertificate(buf, bytes_obtained, cxt);
        break;

      case nsIX509Cert::EMAIL_CERT:
        rv = ImportEmailCertificate(buf, bytes_obtained, cxt);
        break;
      
      default:
        break;
    }
  }

  delete [] buf;
  return rv;  
}

/*
 *  void importPKCS12File(in nsIPK11Token aToken,
 *                        in nsILocalFile aFile);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::ImportPKCS12File(nsIPK11Token *aToken, 
                                     nsILocalFile *aFile)
{
  NS_ENSURE_ARG(aFile);
  nsPKCS12Blob blob;
  blob.SetToken(aToken);
  return blob.ImportFromFile(aFile);
}

/*
 * void exportPKCS12File(in nsIPK11Token aToken,
 *                       in nsILocalFile aFile,
 *                       in PRUint32 count,
 *                       [array, size_is(count)] in wstring aCertNames);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::ExportPKCS12File(nsIPK11Token     *aToken, 
                                     nsILocalFile     *aFile,
                                     PRUint32          count,
                                     nsIX509Cert     **certs)
                                     //const PRUnichar **aCertNames)
{
  NS_ENSURE_ARG(aFile);
  nsPKCS12Blob blob;
  if (count == 0) return NS_OK;
  nsCOMPtr<nsIPK11Token> localRef;
  if (!aToken) {
    PK11SlotInfo *keySlot = PK11_GetInternalKeySlot();
    NS_ASSERTION(keySlot,"Failed to get the internal key slot");
    localRef = new nsPK11Token(keySlot);
    PK11_FreeSlot(keySlot);
    aToken = localRef;
  }
  blob.SetToken(aToken);
  //blob.LoadCerts(aCertNames, count);
  //return blob.ExportToFile(aFile);
  return blob.ExportToFile(aFile, certs, count);
}

/* Header file */
class nsOCSPResponder : public nsIOCSPResponder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOCSPRESPONDER

  nsOCSPResponder();
  nsOCSPResponder(const PRUnichar*, const PRUnichar*);
  virtual ~nsOCSPResponder();
  /* additional members */
  static PRInt32 CmpCAName(nsIOCSPResponder *a, nsIOCSPResponder *b);
  static PRInt32 CompareEntries(nsIOCSPResponder *a, nsIOCSPResponder *b);
  static PRBool IncludeCert(CERTCertificate *aCert);
private:
  nsString mCA;
  nsString mURL;
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsOCSPResponder, nsIOCSPResponder)

nsOCSPResponder::nsOCSPResponder()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsOCSPResponder::nsOCSPResponder(const PRUnichar * aCA, const PRUnichar * aURL)
{
  NS_INIT_ISUPPORTS();
  mCA.Assign(aCA);
  mURL.Assign(aURL);
}

nsOCSPResponder::~nsOCSPResponder()
{
  /* destructor code */
}

/* readonly attribute */
NS_IMETHODIMP nsOCSPResponder::GetResponseSigner(PRUnichar** aCA)
{
  NS_ENSURE_ARG(aCA);
  *aCA = ToNewUnicode(mCA);
  return NS_OK;
}

/* readonly attribute */
NS_IMETHODIMP nsOCSPResponder::GetServiceURL(PRUnichar** aURL)
{
  NS_ENSURE_ARG(aURL);
  *aURL = ToNewUnicode(mURL);
  return NS_OK;
}

PRBool nsOCSPResponder::IncludeCert(CERTCertificate *aCert)
{
  CERTCertTrust *trust;
  char *nickname;

  trust = aCert->trust;
  nickname = aCert->nickname;

  PR_ASSERT(trust != nsnull);

  // Check that trust is non-null //
  if (trust == nsnull) {
    return PR_FALSE;
  }

  if ( ( ( trust->sslFlags & CERTDB_INVISIBLE_CA ) ||
         (trust->emailFlags & CERTDB_INVISIBLE_CA ) ||
         (trust->objectSigningFlags & CERTDB_INVISIBLE_CA ) ) ||
       nickname == NULL) {
      return PR_FALSE;
  }
  if ((trust->sslFlags & CERTDB_VALID_CA) ||
      (trust->emailFlags & CERTDB_VALID_CA) ||
      (trust->objectSigningFlags & CERTDB_VALID_CA)) {
      return PR_TRUE;
  }
  return PR_FALSE;
}

// CmpByCAName
//
// Compare two responders their token name.  Returns -1, 0, 1 as
// in strcmp.  No token name (null) is treated as >.
PRInt32 nsOCSPResponder::CmpCAName(nsIOCSPResponder *a, nsIOCSPResponder *b)
{
  PRInt32 cmp1;
  nsXPIDLString aTok, bTok;
  a->GetResponseSigner(getter_Copies(aTok));
  b->GetResponseSigner(getter_Copies(bTok));
  if (aTok != nsnull && bTok != nsnull) {
    cmp1 = Compare(aTok, bTok);
  } else {
    cmp1 = (aTok == nsnull) ? 1 : -1;
  }
  return cmp1;
}

// ocsp_compare_entries
//
// Compare two responders.  Returns -1, 0, 1 as
// in strcmp.  Entries with urls come before those without urls.
PRInt32 nsOCSPResponder::CompareEntries(nsIOCSPResponder *a, nsIOCSPResponder *b)
{
  nsXPIDLString aURL, bURL;
  nsAutoString aURLAuto, bURLAuto;

  a->GetServiceURL(getter_Copies(aURL));
  aURLAuto.Assign(aURL);
  b->GetServiceURL(getter_Copies(bURL));
  bURLAuto.Assign(bURL);

  if (aURLAuto.Length() > 0 ) {
    if (bURLAuto.Length() > 0) {
      return nsOCSPResponder::CmpCAName(a, b);
    } else {
      return -1;
    }
  } else {
    if (bURLAuto.Length() > 0) {
      return 1;
    } else {
      return nsOCSPResponder::CmpCAName(a, b);
    }
  }
}

static SECStatus PR_CALLBACK 
GetOCSPResponders (CERTCertificate *aCert,
                   SECItem         *aDBKey,
                   void            *aArg)
{
  nsISupportsArray *array = NS_STATIC_CAST(nsISupportsArray*, aArg);
  PRUnichar* nn = nsnull;
  PRUnichar* url = nsnull;
  char *serviceURL = nsnull;
  char *nickname = nsnull;
  PRUint32 i, count;
  nsresult rv;

  // Are we interested in this cert //
  if (!nsOCSPResponder::IncludeCert(aCert)) {
    return SECSuccess;
  }

  // Get the AIA and nickname //
  serviceURL = CERT_GetOCSPAuthorityInfoAccessLocation(aCert);
  if (serviceURL) {
	url = ToNewUnicode(NS_ConvertUTF8toUCS2(serviceURL));
  }

  nickname = aCert->nickname;
  nn = ToNewUnicode(NS_ConvertUTF8toUCS2(nickname));

  nsCOMPtr<nsIOCSPResponder> new_entry = new nsOCSPResponder(nn, url);

  // Sort the items according to nickname //
  rv = array->Count(&count);
  for (i=0; i < count; ++i) {
    nsCOMPtr<nsISupports> isupport = dont_AddRef(array->ElementAt(i));
    nsCOMPtr<nsIOCSPResponder> entry = do_QueryInterface(isupport);
    if (nsOCSPResponder::CompareEntries(new_entry, entry) < 0) {
      array->InsertElementAt(new_entry, i);
      break;
    }
  }
  if (i == count) {
    array->AppendElement(new_entry);
  }
  return SECSuccess;
}

/*
 * getOCSPResponders
 *
 * Export a set of certs and keys from the database to a PKCS#12 file.
*/
NS_IMETHODIMP 
nsNSSCertificateDB::GetOCSPResponders(nsISupportsArray ** aResponders)
{
  SECStatus sec_rv;
  nsCOMPtr<nsISupportsArray> respondersArray;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(respondersArray));
  if (NS_FAILED(rv)) {
    return rv;
  }

  sec_rv = PK11_TraverseSlotCerts(::GetOCSPResponders,
                                  respondersArray,
                                  nsnull);
  if (sec_rv != SECSuccess) {
    goto loser;
  }

  *aResponders = respondersArray;
  NS_IF_ADDREF(*aResponders);
  return NS_OK;
loser:
  return NS_ERROR_FAILURE;
}

/*
 * NSS Helper Routines (private to nsNSSCertificateDB)
 */

#define DELIM '\001'

/*
 * GetSortedNameList
 *
 * Converts a CERTCertList to a list of certificate names
 */
void
nsNSSCertificateDB::getCertNames(CERTCertList *certList,
                                 PRUint32      type, 
                                 PRUint32     *_count,
                                 PRUnichar  ***_certNames)
{
  nsresult rv;
  CERTCertListNode *node;
  PRUint32 numcerts = 0, i=0;
  PRUnichar **tmpArray = NULL;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("List of certs %d:\n", type));
  for (node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList);
       node = CERT_LIST_NEXT(node)) {
    if (getCertType(node->cert) == type) {
      numcerts++;
    }
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("num certs: %d\n", numcerts));
  int nc = (numcerts == 0) ? 1 : numcerts;
  tmpArray = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * nc);
  if (numcerts == 0) goto finish;
  for (node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList);
       node = CERT_LIST_NEXT(node)) {
    if (getCertType(node->cert) == type) {
      nsNSSCertificate pipCert(node->cert);
      char *dbkey = NULL;
      char *namestr = NULL;
      nsAutoString certstr;
      rv = pipCert.GetDbKey(&dbkey);
      nsAutoString keystr = NS_ConvertASCIItoUCS2(dbkey);
      PR_FREEIF(dbkey);
      if (type == nsIX509Cert::EMAIL_CERT) {
        namestr = node->cert->emailAddr;
      } else {
        namestr = node->cert->nickname;
        char *sc = strchr(namestr, ':');
        if (sc) *sc = DELIM;
      }
      nsAutoString certname = NS_ConvertASCIItoUCS2(namestr);
      certstr.Append(PRUnichar(DELIM));
      certstr += certname;
      certstr.Append(PRUnichar(DELIM));
      certstr += keystr;
      tmpArray[i++] = ToNewUnicode(certstr);
    }
  }
finish:
  *_count = numcerts;
  *_certNames = tmpArray;
}

/* somewhat follows logic of cert_list_include_cert from PSM 1.x */
PRUint32 
nsNSSCertificateDB::getCertType(CERTCertificate *cert)
{
  char *nick = cert->nickname;
  char *email = cert->emailAddr;
  nsNSSCertTrust trust(cert->trust);
  /*
fprintf(stderr, "====> nick: %s  email: %s  has-any-user: %d  hash-any-ca: %d  has-peer100: %d  has-peer001: %d\n",
  nick, email, (nick) ? trust.HasAnyUser() : 0, (nick) ? trust.HasAnyCA() : 0, (nick) ? trust.HasPeer(PR_TRUE, PR_FALSE, PR_FALSE) : 0, 
  (email) ? trust.HasPeer(PR_FALSE, PR_TRUE, PR_FALSE) : 0 );
*/
  if (nick) {
    if (trust.HasAnyUser())
      return nsIX509Cert::USER_CERT;
    if (trust.HasAnyCA() || CERT_IsCACert(cert,NULL))
      return nsIX509Cert::CA_CERT;
    if (trust.HasPeer(PR_TRUE, PR_FALSE, PR_FALSE))
      return nsIX509Cert::SERVER_CERT;
  }
  if (email && trust.HasPeer(PR_FALSE, PR_TRUE, PR_FALSE))
    return nsIX509Cert::EMAIL_CERT;
  return nsIX509Cert::UNKNOWN_CERT;
}


NS_IMETHODIMP 
nsNSSCertificateDB::ImportCrl (char *aData, PRUint32 aLength, nsIURI * aURI, PRUint32 aType, PRBool doSilentDonwload, const PRUnichar* crlKey)
{
  nsresult rv;
  PRArenaPool *arena = NULL;
  CERTCertificate *caCert;
  SECItem derName = { siBuffer, NULL, 0 };
  SECItem derCrl;
  CERTSignedData sd;
  SECStatus sec_rv;
  CERTSignedCrl *crl;
  nsCAutoString url;
  nsCOMPtr<nsICrlEntry> crlEntry;
  PRBool importSuccessful;
  PRInt32 errorCode;
  nsString errorMessage;
  
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv)) return rv;
	         
  aURI->GetSpec(url);
  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena) {
    goto loser;
  }
  memset(&sd, 0, sizeof(sd));

  derCrl.data = (unsigned char*)aData;
  derCrl.len = aLength;
  sec_rv = CERT_KeyFromDERCrl(arena, &derCrl, &derName);
  if (sec_rv != SECSuccess) {
    goto loser;
  }

  caCert = CERT_FindCertByName(CERT_GetDefaultCertDB(), &derName);
  if (!caCert) {
    if (aType == SEC_KRL_TYPE){
      goto loser;
    }
  } else {
    sec_rv = SEC_ASN1DecodeItem(arena,
                            &sd, SEC_ASN1_GET(CERT_SignedDataTemplate), 
                            &derCrl);
    if (sec_rv != SECSuccess) {
      goto loser;
    }
    sec_rv = CERT_VerifySignedData(&sd, caCert, PR_Now(),
                               nsnull);
    if (sec_rv != SECSuccess) {
      goto loser;
    }
  }
  
  crl = SEC_NewCrl(CERT_GetDefaultCertDB(), (char*)url.get(), &derCrl,
                   aType);
  
  if (!crl) {
    goto loser;
  }

  crlEntry = new nsCrlEntry(crl);
  SSL_ClearSessionCache();
  SEC_DestroyCrl(crl);
  
  importSuccessful = PR_TRUE;
  goto done;

loser:
  importSuccessful = PR_FALSE;
  errorCode = PR_GetError();
  switch (errorCode) {
    case SEC_ERROR_CRL_EXPIRED:
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CrlImportFailureExpired").get(), errorMessage);
      break;

	case SEC_ERROR_CRL_BAD_SIGNATURE:
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CrlImportFailureBadSignature").get(), errorMessage);
      break;

	case SEC_ERROR_CRL_INVALID:
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CrlImportFailureInvalid").get(), errorMessage);
      break;

	case SEC_ERROR_OLD_CRL:
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CrlImportFailureOld").get(), errorMessage);
      break;

	case SEC_ERROR_CRL_NOT_YET_VALID:
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CrlImportFailureNotYetValid").get(), errorMessage);
      break;

    default:
      nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CrlImportFailureReasonUnknown").get(), errorMessage);
      errorMessage.AppendInt(errorCode,16);
      break;
  }

done:
          
  if(!doSilentDonwload){
    if (!importSuccessful){
      nsString message;
      nsString temp;
      nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      nsCOMPtr<nsIPrompt> prompter;
      if (wwatch){
        wwatch->GetNewPrompter(0, getter_AddRefs(prompter));
        nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CrlImportFailure1").get(), message);
        message.Append(NS_LITERAL_STRING("\n").get());
        message.Append(errorMessage);
        nssComponent->GetPIPNSSBundleString(NS_LITERAL_STRING("CrlImportFailure2").get(), temp);
        message.Append(NS_LITERAL_STRING("\n").get());
        message.Append(temp);
     
        if(prompter)
          prompter->Alert(0, message.get());
      }
    } else {
      nsCOMPtr<nsICertificateDialogs> certDialogs;
      // Not being able to display the success dialog should not
      // be a fatal error, so don't return a failure code.
      if (NS_SUCCEEDED(::getNSSDialogs(getter_AddRefs(certDialogs),
				       NS_GET_IID(nsICertificateDialogs)))) {
	nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();
	certDialogs->CrlImportStatusDialog(cxt, crlEntry);
      }
    }
  } else {
    if(crlKey == nsnull){
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID,&rv);
    if (NS_FAILED(rv)){
      return rv;
    }
    
    nsCAutoString updateErrCntPrefStr(CRL_AUTOUPDATE_ERRCNT_PREF);
    updateErrCntPrefStr.AppendWithConversion(crlKey);
    if(importSuccessful){
      PRUnichar *updateTime;
      nsCAutoString updateTimeStr;
      PRUnichar *updateURL;
      nsCAutoString updateURLStr;
      PRInt32 timingTypePref;
      double dayCnt;
      char *dayCntStr;
      nsCAutoString updateTypePrefStr(CRL_AUTOUPDATE_TIMIINGTYPE_PREF);
      nsCAutoString updateTimePrefStr(CRL_AUTOUPDATE_TIME_PREF);
      nsCAutoString updateUrlPrefStr(CRL_AUTOUPDATE_URL_PREF);
      nsCAutoString updateDayCntPrefStr(CRL_AUTOUPDATE_DAYCNT_PREF);
      nsCAutoString updateFreqCntPrefStr(CRL_AUTOUPDATE_FREQCNT_PREF);
      updateTypePrefStr.AppendWithConversion(crlKey);
      updateTimePrefStr.AppendWithConversion(crlKey);
      updateUrlPrefStr.AppendWithConversion(crlKey);
      updateDayCntPrefStr.AppendWithConversion(crlKey);
      updateFreqCntPrefStr.AppendWithConversion(crlKey);

      pref->GetIntPref(updateTypePrefStr.get(),&timingTypePref);
      
      //Compute and update the next download instant
      if(timingTypePref == nsCrlEntry::TYPE_AUTOUPDATE_TIME_BASED){
        pref->GetCharPref(updateDayCntPrefStr.get(),&dayCntStr);
      }else{
        pref->GetCharPref(updateFreqCntPrefStr.get(),&dayCntStr);
      }
      dayCnt = atof(dayCntStr);
      nsMemory::Free(dayCntStr);

      PRBool toBeRescheduled = PR_FALSE;
      if(NS_SUCCEEDED(crlEntry->ComputeNextAutoUpdateTime(timingTypePref,dayCnt,&updateTime))){
        updateTimeStr.AssignWithConversion(updateTime);
        nsMemory::Free(updateTime);
        pref->SetCharPref(updateTimePrefStr.get(),updateTimeStr.get());
        //Now, check if this update time is already in the past. This would
        //imply we have downloaded the same crl, or there is something wrong
        //with the next update date. We will not reschedule this crl in this
        //session anymore - or else, we land into a loop. It would anyway be
        //imported once the browser is restarted.
        PRTime nextTime;
        PR_ParseTimeString(updateTimeStr.get(),PR_TRUE, &nextTime);
        if(LL_CMP(nextTime, > , PR_Now())){
          toBeRescheduled = PR_TRUE;
        }
      }
      
      //Update the url to download from, next time
      crlEntry->GetLastFetchURL(&updateURL);
      updateURLStr.AssignWithConversion(updateURL);
      nsMemory::Free(updateURL);
      pref->SetCharPref(updateUrlPrefStr.get(),updateURLStr.get());
      
      pref->SetIntPref(updateErrCntPrefStr.get(),0);
      pref->SavePrefFile(nsnull);
      
      if(toBeRescheduled == PR_TRUE){
        nsAutoString hashKey(crlKey);
        nssComponent->RemoveCrlFromList(hashKey);
        nssComponent->DefineNextTimer();
      }

    } else{
      PRInt32 errCnt;
      nsCAutoString errMsg;
      nsCAutoString updateErrDetailPrefStr(CRL_AUTOUPDATE_ERRDETAIL_PREF);
      updateErrDetailPrefStr.AppendWithConversion(crlKey);
      errMsg.AssignWithConversion(errorMessage.get());
      rv = pref->GetIntPref(updateErrCntPrefStr.get(),&errCnt);
      if( (NS_FAILED(rv)) || (errCnt ==0)){
        pref->SetIntPref(updateErrCntPrefStr.get(),1);
      }else{
        pref->SetIntPref(updateErrCntPrefStr.get(),errCnt+1);
      }
      pref->SetCharPref(updateErrDetailPrefStr.get(),errMsg.get());
      pref->SavePrefFile(nsnull);
    }
  }

  return rv;
}

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsCrlEntry, nsICrlEntry)

nsCrlEntry::nsCrlEntry()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsCrlEntry::nsCrlEntry(const PRUnichar * aOrg, const PRUnichar * aOrgUnit, 
               const PRUnichar * aLastUpdateLocale, const PRUnichar * aNextUpdateLocale, 
               PRTime aLastUpdate, PRTime aNextUpdate, const PRUnichar *aNameInDb,
               const PRUnichar *aLastFetchURL)
{
  NS_INIT_ISUPPORTS();
  mOrg.Assign(aOrg);
  mOrgUnit.Assign(aOrgUnit);
  mLastUpdateLocale.Assign(aLastUpdateLocale);
  mNextUpdateLocale.Assign(aNextUpdateLocale);
  mLastUpdate = aLastUpdate;
  mNextUpdate = aNextUpdate;
  mNameInDb.Assign(aNameInDb);
  mLastFetchURL.Assign(aLastFetchURL);
}


nsCrlEntry::nsCrlEntry(CERTSignedCrl *signedCrl)
{
  NS_INIT_ISUPPORTS();
  
  CERTCrl *crl = &(signedCrl->crl);
  nsAutoString org;
  nsAutoString orgUnit;
  nsAutoString nameInDb;
  nsAutoString nextUpdateLocale;
  nsAutoString lastUpdateLocale;
  nsAutoString lastFetchURL;
  PRTime lastUpdate;
  PRTime nextUpdate;
  SECStatus sec_rv;
  
  // Get the information we need here //
  char * o = CERT_GetOrgName(&(crl->name));
  if (o) {
    org = NS_ConvertASCIItoUCS2(o);
    PORT_Free(o);
  }

  char * ou = CERT_GetOrgUnitName(&(crl->name));
  if (ou) {
    orgUnit = NS_ConvertASCIItoUCS2(ou);
    //At present, the ou is being used as the unique key - but this
    //would change, one support for delta crls come in.
    nameInDb =  orgUnit;
    PORT_Free(ou);
  }
  
  nsCOMPtr<nsIDateTimeFormat> dateFormatter = do_CreateInstance(kDateTimeFormatCID);
  
  // Last Update time
  if (crl->lastUpdate.len) {
    sec_rv = DER_UTCTimeToTime(&lastUpdate, &(crl->lastUpdate));
    if (sec_rv == SECSuccess && dateFormatter) {
      dateFormatter->FormatPRTime(nsnull, kDateFormatShort, kTimeFormatNone,
                            lastUpdate, lastUpdateLocale);
    }
  }

  if (crl->nextUpdate.len) {
    // Next update time
    sec_rv = DER_UTCTimeToTime(&nextUpdate, &(crl->nextUpdate));
    if (sec_rv == SECSuccess && dateFormatter) {
      dateFormatter->FormatPRTime(nsnull, kDateFormatShort, kTimeFormatNone,
                            nextUpdate, nextUpdateLocale);
    }
  }

  char * url = signedCrl->url;
  if(url) {
    lastFetchURL =  NS_ConvertASCIItoUCS2(url);
  }

  mOrg.Assign(org.get());
  mOrgUnit.Assign(orgUnit.get());
  mLastUpdateLocale.Assign(lastUpdateLocale.get());
  mNextUpdateLocale.Assign(nextUpdateLocale.get());
  mLastUpdate = lastUpdate;
  mNextUpdate = nextUpdate;
  mNameInDb.Assign(nameInDb.get());
  mLastFetchURL.Assign(lastFetchURL.get());
    
}

nsCrlEntry::~nsCrlEntry()
{
  /* destructor code */
}

/* readonly attribute */
NS_IMETHODIMP nsCrlEntry::GetOrg(PRUnichar** aOrg)
{
  NS_ENSURE_ARG(aOrg);
  *aOrg = ToNewUnicode(mOrg);
  return NS_OK;
}

/* readonly attribute */
NS_IMETHODIMP nsCrlEntry::GetOrgUnit(PRUnichar** aOrgUnit)
{
  NS_ENSURE_ARG(aOrgUnit);
  *aOrgUnit = ToNewUnicode(mOrgUnit);
  return NS_OK;
}

NS_IMETHODIMP nsCrlEntry::GetLastUpdateLocale(PRUnichar** aLastUpdateLocale)
{
  NS_ENSURE_ARG(aLastUpdateLocale);
  *aLastUpdateLocale = ToNewUnicode(mLastUpdateLocale);
  return NS_OK;
}

NS_IMETHODIMP nsCrlEntry::GetNextUpdateLocale(PRUnichar** aNextUpdateLocale)
{
  NS_ENSURE_ARG(aNextUpdateLocale);
  *aNextUpdateLocale = ToNewUnicode(mNextUpdateLocale);
  return NS_OK;
}


/* readonly attribute */
NS_IMETHODIMP nsCrlEntry::GetNameInDb(PRUnichar** aNameInDb)
{
  NS_ENSURE_ARG(aNameInDb);
  *aNameInDb = ToNewUnicode(mNameInDb);
  return NS_OK;
}

NS_IMETHODIMP nsCrlEntry::GetLastFetchURL(PRUnichar** aLastFetchURL)
{
  NS_ENSURE_ARG(aLastFetchURL);
  *aLastFetchURL = ToNewUnicode(mLastFetchURL);
  return NS_OK;
}

NS_IMETHODIMP nsCrlEntry::ComputeNextAutoUpdateTime(PRUint32 autoUpdateType, double dayCnt, PRUnichar **nextAutoUpdate)
{

  PRTime microsecInDayCnt;
  PRTime now = PR_Now();
  PRTime tempTime;
  PRInt64 diff = 0;
  PRInt64 secsInDay = 86400UL;
  PRInt64 temp;
  PRInt64 cycleCnt = 0;
  PRInt64 secsInDayCnt;
  PRFloat64 tmpData;
  
  LL_L2F(tmpData,secsInDay);
  LL_MUL(tmpData,dayCnt,tmpData);
  LL_F2L(secsInDayCnt,tmpData);
  LL_MUL(microsecInDayCnt, secsInDayCnt, PR_USEC_PER_SEC);
    
  switch (autoUpdateType) {
  case nsCrlEntry::TYPE_AUTOUPDATE_FREQ_BASED:
    LL_SUB(diff, now, mLastUpdate);             //diff is the no of micro sec between now and last update
    LL_DIV(cycleCnt, diff, microsecInDayCnt);   //temp is the number of full cycles from lst update
    LL_MOD(temp, diff, microsecInDayCnt);
    if(!(LL_IS_ZERO(temp))) {
      LL_ADD(cycleCnt,cycleCnt,1);            //no of complete cycles till next autoupdate instant
    }
    LL_MUL(temp,cycleCnt,microsecInDayCnt);    //micro secs from last update
    LL_ADD(tempTime, mLastUpdate, temp);
    break;  
  case nsCrlEntry::TYPE_AUTOUPDATE_TIME_BASED:
    LL_SUB(tempTime, mNextUpdate, microsecInDayCnt);
    break;
  default:
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  //Now, a basic constraing is that the next auto update date can never be after
  //next update, if one is defined
  if(LL_CMP(mNextUpdate , > , 0 )) {
    if(LL_CMP(tempTime , > , mNextUpdate)) {
      tempTime = mNextUpdate;
    }
  }

  nsAutoString nextAutoUpdateDate;
  PRExplodedTime explodedTime;
  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter = do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv))
    return rv;
  PR_ExplodeTime(tempTime, PR_GMTParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSeconds,
                                      &explodedTime, nextAutoUpdateDate);
  *nextAutoUpdate = ToNewUnicode(nextAutoUpdateDate);

  return NS_OK;
}

NS_IMETHODIMP 
nsNSSCertificateDB::UpdateCRLFromURL( const PRUnichar *url, const PRUnichar* key, PRBool *res)
{
  nsresult rv;
  nsAutoString downloadUrl(url);
  nsAutoString dbKey(key);
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if(NS_FAILED(rv)){
    *res = PR_FALSE;
    return rv;
  }

  rv = nssComponent->DownloadCRLDirectly(downloadUrl, dbKey);
  if(NS_FAILED(rv)){
    *res = PR_FALSE;
  } else {
    *res = PR_TRUE;
  }
  return NS_OK;

}

NS_IMETHODIMP 
nsNSSCertificateDB::RescheduleCRLAutoUpdate(void)
{
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if(NS_FAILED(rv)){
    return rv;
  }
  rv = nssComponent->DefineNextTimer();
  return rv;
}

/*
 * getCRLs
 *
 * Export a set of certs and keys from the database to a PKCS#12 file.
*/

NS_IMETHODIMP 
nsNSSCertificateDB::GetCrls(nsISupportsArray ** aCrls)
{
  SECStatus sec_rv;
  CERTCrlHeadNode *head = nsnull;
  CERTCrlNode *node = nsnull;
  nsCOMPtr<nsISupportsArray> crlsArray;
  nsresult rv;
  rv = NS_NewISupportsArray(getter_AddRefs(crlsArray));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Get the list of certs //
  sec_rv = SEC_LookupCrls(CERT_GetDefaultCertDB(), &head, -1);
  if (sec_rv != SECSuccess) {
    goto loser;
  }

  if (head) {
    for (node=head->first; node != nsnull; node = node->next) {

      nsCOMPtr<nsICrlEntry> entry = new nsCrlEntry((node->crl));
      crlsArray->AppendElement(entry);
    }
    PORT_FreeArena(head->arena, PR_FALSE);
  }

  *aCrls = crlsArray;
  NS_IF_ADDREF(*aCrls);
  return NS_OK;
loser:
  return NS_ERROR_FAILURE;;
}

/*
 * deletetCrl
 *
 * Delete a Crl entry from the cert db.
*/
NS_IMETHODIMP 
nsNSSCertificateDB::DeleteCrl(PRUint32 aCrlIndex)
{
  CERTSignedCrl *realCrl = nsnull;
  CERTCrlHeadNode *head = nsnull;
  CERTCrlNode *node = nsnull;
  SECStatus sec_rv;
  PRUint32 i;

  // Get the list of certs //
  sec_rv = SEC_LookupCrls(CERT_GetDefaultCertDB(), &head, -1);
  if (sec_rv != SECSuccess) {
    goto loser;
  }

  if (head) {
    for (i = 0, node=head->first; node != nsnull; i++, node = node->next) {
      if (i != aCrlIndex) {
        continue;
      }
      realCrl = SEC_FindCrlByName(CERT_GetDefaultCertDB(), &(node->crl->crl.derName), node->type);
      SEC_DeletePermCRL(realCrl);
      SEC_DestroyCrl(realCrl);
      SSL_ClearSessionCache();
    }
    PORT_FreeArena(head->arena, PR_FALSE);
  }
  return NS_OK;
loser:
  return NS_ERROR_FAILURE;;
}

/* readonly attribute boolean ocspOn; */
NS_IMETHODIMP 
nsNSSCertificateDB::GetOcspOn(PRBool *aOcspOn)
{
  nsCOMPtr<nsIPref> prefService = do_GetService(NS_PREF_CONTRACTID);

  PRInt32 ocspEnabled;
  prefService->GetIntPref("security.OCSP.enabled", &ocspEnabled);
  *aOcspOn = ( ocspEnabled == 0 ) ? PR_FALSE : PR_TRUE; 
  return NS_OK;
}

/* void disableOCSP (); */
NS_IMETHODIMP 
nsNSSCertificateDB::DisableOCSP()
{
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  return nssComponent->DisableOCSP();
}

/* void enableOCSP (); */
NS_IMETHODIMP 
nsNSSCertificateDB::EnableOCSP()
{
  nsresult rv;
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
  if (NS_FAILED(rv))
    return rv;

  return nssComponent->EnableOCSP();
}

/* nsIX509Cert getDefaultEmailEncryptionCert (); */
NS_IMETHODIMP
nsNSSCertificateDB::GetEmailEncryptionCert(const PRUnichar* aNickname, nsIX509Cert **_retval)
{
  if (!aNickname || !_retval)
    return NS_ERROR_FAILURE;

  *_retval = 0;

  nsDependentString aDepNickname(aNickname);
  if (aDepNickname.IsEmpty())
    return NS_OK;

  nsresult rv = NS_OK;
  CERTCertificate *cert = 0;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsNSSCertificate *nssCert = nsnull;
  char *asciiname = NULL;
  NS_ConvertUCS2toUTF8 aUtf8Nickname(aDepNickname);
  asciiname = NS_CONST_CAST(char*, aUtf8Nickname.get());

  /* Find a good cert in the user's database */
  cert = CERT_FindUserCertByUsage(CERT_GetDefaultCertDB(), asciiname, 
           certUsageEmailRecipient, PR_TRUE, ctx);

  if (!cert) { goto loser; }  

  nssCert = new nsNSSCertificate(cert);
  if (nssCert == nsnull) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(nssCert);

  *_retval = NS_STATIC_CAST(nsIX509Cert*, nssCert);

loser:
  if (cert) CERT_DestroyCertificate(cert);
  return rv;
}

/* nsIX509Cert getDefaultEmailSigningCert (); */
NS_IMETHODIMP
nsNSSCertificateDB::GetEmailSigningCert(const PRUnichar* aNickname, nsIX509Cert **_retval)
{
  if (!aNickname || !_retval)
    return NS_ERROR_FAILURE;

  *_retval = 0;

  nsDependentString aDepNickname(aNickname);
  if (aDepNickname.IsEmpty())
    return NS_OK;

  nsresult rv = NS_OK;
  CERTCertificate *cert = 0;
  nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
  nsNSSCertificate *nssCert = nsnull;
  char *asciiname = NULL;
  NS_ConvertUCS2toUTF8 aUtf8Nickname(aDepNickname);
  asciiname = NS_CONST_CAST(char*, aUtf8Nickname.get());

  /* Find a good cert in the user's database */
  cert = CERT_FindUserCertByUsage(CERT_GetDefaultCertDB(), asciiname, 
           certUsageEmailSigner, PR_TRUE, ctx);

  if (!cert) { goto loser; }  

  nssCert = new nsNSSCertificate(cert);
  if (nssCert == nsnull) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(nssCert);

  *_retval = NS_STATIC_CAST(nsIX509Cert*, nssCert);

loser:
  if (cert) CERT_DestroyCertificate(cert);
  return rv;
}

/* nsIX509Cert getCertByEmailAddress (in nsIPK11Token aToken, in wstring aEmailAddress); */
NS_IMETHODIMP
nsNSSCertificateDB::GetCertByEmailAddress(nsIPK11Token *aToken, const char *aEmailAddress, nsIX509Cert **_retval)
{
  CERTCertificate *any_cert = CERT_FindCertByNicknameOrEmailAddr(CERT_GetDefaultCertDB(), (char*)aEmailAddress);
  if (!any_cert)
    return NS_ERROR_FAILURE;

  CERTCertificateCleaner certCleaner(any_cert);
    
  // any_cert now contains a cert with the right subject, but it might not have the correct usage
  CERTCertList *certlist = CERT_CreateSubjectCertList(
    nsnull, CERT_GetDefaultCertDB(), &any_cert->derSubject, PR_Now(), PR_TRUE);
  if (!certlist)
    return NS_ERROR_FAILURE;  

  CERTCertListCleaner listCleaner(certlist);

  if (SECSuccess != CERT_FilterCertListByUsage(certlist, certUsageEmailRecipient, PR_FALSE))
    return NS_ERROR_FAILURE;
  
  if (CERT_LIST_END(CERT_LIST_HEAD(certlist), certlist))
    return NS_ERROR_FAILURE;
  
  nsNSSCertificate *nssCert = new nsNSSCertificate(CERT_LIST_HEAD(certlist)->cert);
  if (!nssCert)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(nssCert);
  *_retval = NS_STATIC_CAST(nsIX509Cert*, nssCert);
  return NS_OK;
}

/* nsIX509Cert constructX509FromBase64 (in string base64); */
NS_IMETHODIMP
nsNSSCertificateDB::ConstructX509FromBase64(const char * base64, nsIX509Cert **_retval)
{
  if (!_retval) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 len = PL_strlen(base64);
  int adjust = 0;

  /* Compute length adjustment */
  if (base64[len-1] == '=') {
    adjust++;
    if (base64[len-2] == '=') adjust++;
  }

  nsresult rv = NS_OK;
  char *certDER = 0;
  PRInt32 lengthDER = 0;

  certDER = PL_Base64Decode(base64, len, NULL);
  if (!certDER || !*certDER) {
    rv = NS_ERROR_ILLEGAL_VALUE;
  }
  else {
    lengthDER = (len*3)/4 - adjust;

    SECItem secitem_cert;
    secitem_cert.type = siDERCertBuffer;
    secitem_cert.data = (unsigned char*)certDER;
    secitem_cert.len = lengthDER;

    CERTCertificate *cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &secitem_cert, nsnull, PR_FALSE, PR_TRUE);

    if (!cert) {
      rv = NS_ERROR_FAILURE;
    }
    else {
      nsNSSCertificate *nsNSS = new nsNSSCertificate(cert);
      if (!nsNSS) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
      else {
        nsresult rv = nsNSS->QueryInterface(NS_GET_IID(nsIX509Cert), (void**)_retval);

        if (NS_SUCCEEDED(rv) && *_retval) {
          NS_ADDREF(*_retval);
        }
        
        NS_RELEASE(nsNSS);
      }
      CERT_DestroyCertificate(cert);
    }
  }
  
  if (certDER) {
    nsCRT::free(certDER);
  }
  return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsNSSCertCache, nsINSSCertCache)

nsNSSCertCache::nsNSSCertCache()
:mCertList(nsnull)
{
  mutex = PR_NewLock();
}

nsNSSCertCache::~nsNSSCertCache()
{
  if (mCertList) {
    CERT_DestroyCertList(mCertList);
  }
  if (mutex) {
    PR_DestroyLock(mutex);
    mutex = nsnull;
  }
}

NS_IMETHODIMP
nsNSSCertCache::CacheAllCerts()
{
  {
    nsAutoLock lock(mutex);
    if (mCertList) {
      CERT_DestroyCertList(mCertList);
      mCertList = nsnull;
    }
  }

  nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();
  
  CERTCertList *newList = PK11_ListCerts(PK11CertListUnique, cxt);

  if (newList) {
    nsAutoLock lock(mutex);
    mCertList = newList;
  }
  
  return NS_OK;
}

void* nsNSSCertCache::GetCachedCerts()
{
  nsAutoLock lock(mutex);
  return mCertList;
}

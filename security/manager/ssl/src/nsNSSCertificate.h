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

#ifndef _NS_NSSCERTIFICATE_H_
#define _NS_NSSCERTIFICATE_H_

#include "nsIX509Cert.h"
#include "nsIX509CertDB.h"

#ifdef NSS_3_4
/* private NSS defines used by PSM */
/* (must be declated before cert.h) */
#define CERT_NewTempCertificate __CERT_NewTempCertificate
#define CERT_AddTempCertToPerm __CERT_AddTempCertToPerm
#endif

#include "prtypes.h"
#include "cert.h"
#include "secitem.h"
#include "nsString.h"


class nsINSSComponent;

/* Certificate */
class nsNSSCertificate : public nsIX509Cert 
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIX509CERT

  nsNSSCertificate(CERTCertificate *cert);
  /* from a request? */
  virtual ~nsNSSCertificate();
  CERTCertificate *GetCert();
  nsresult MarkForPermDeletion();
  nsresult SetCertType(PRUint32 aCertType);
  nsresult GetCertType(PRUint32 *aCertType);
  nsresult FormatUIStrings(const nsAutoString &nickname, nsAutoString &nickWithSerial, nsAutoString &details);
  static nsNSSCertificate* ConstructFromDER(char *certDER, int derLen);

  static char* defaultServerNickname(CERTCertificate* cert);

private:
  CERTCertificate *mCert;
  PRBool           mPermDelete;
  PRUint32         mCertType;
  nsCOMPtr<nsIASN1Object> mASN1Structure;
  nsresult CreateASN1Struct();
  nsresult CreateTBSCertificateASN1Struct(nsIASN1Sequence **retSequence,
                                          nsINSSComponent *nssComponent);
};

/* Header file */
#define CRL_AUTOUPDATE_TIMIINGTYPE_PREF "security.crl.autoupdate.timingType"
#define CRL_AUTOUPDATE_TIME_PREF "security.crl.autoupdate.nextInstant"
#define CRL_AUTOUPDATE_URL_PREF "security.crl.autoupdate.url"
#define CRL_AUTOUPDATE_DAYCNT_PREF "security.crl.autoupdate.dayCnt"
#define CRL_AUTOUPDATE_FREQCNT_PREF "security.crl.autoupdate.freqCnt"
#define CRL_AUTOUPDATE_ERRCNT_PREF "security.crl.autoupdate.errCount"
#define CRL_AUTOUPDATE_ERRDETAIL_PREF "security.crl.autoupdate.errDetail"
#define CRL_AUTOUPDATE_ENABLED_PREF "security.crl.autoupdate.enable."
#define CRL_AUTOUPDATE_DEFAULT_DELAY 30000UL
	  
class nsCrlEntry : public nsICrlEntry
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRLENTRY

  nsCrlEntry();
  nsCrlEntry(CERTSignedCrl *);
  nsCrlEntry(const PRUnichar*, const PRUnichar*, const PRUnichar*, const PRUnichar*, PRTime, PRTime, const PRUnichar*, const PRUnichar*);
  virtual ~nsCrlEntry();
  /* additional members */
private:
  nsString mOrg;
  nsString mOrgUnit;
  nsString mLastUpdateLocale;
  nsString mNextUpdateLocale;
  PRTime mLastUpdate;
  PRTime mNextUpdate;
  nsString mNameInDb;
  nsString mLastFetchURL;
  nsString mNextAutoUpdateDate;
};


class nsNSSCertificateDB : public nsIX509CertDB
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIX509CERTDB

  nsNSSCertificateDB(); 
  virtual ~nsNSSCertificateDB();

  static PRUint32 getCertType(CERTCertificate *cert);

private:

  void getCertNames(CERTCertList *certList,
                    PRUint32      type, 
                    PRUint32     *_count,
                    PRUnichar  ***_certNameList);

  CERTDERCerts *getCertsFromPackage(PRArenaPool *arena, char *data, 
                                    PRUint32 length);
  nsresult handleCACertDownload(nsISupportsArray *x509Certs, 
                                nsIInterfaceRequestor *ctx);
};

// Use this function to generate a default nickname for a user
// certificate that is to be imported onto a token.
char *
default_nickname(CERTCertificate *cert, nsIInterfaceRequestor* ctx);


#endif /* _NS_NSSCERTIFICATE_H_ */

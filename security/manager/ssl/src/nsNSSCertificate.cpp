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
 * $Id: nsNSSCertificate.cpp,v 1.10 2001/03/21 03:37:48 javi%netscape.com Exp $
 */

#include "prmem.h"

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsILocalFile.h"
#include "nsNSSCertificate.h"
#include "nsPKCS12Blob.h"
#include "nsIX509Cert.h"
#include "nsString.h"

#include "pk11func.h"
#include "certdb.h"
#include "cert.h"
#include "nssb64.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

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

  /* common defaults */
  /* equivalent to "c,c,c" */
  void SetValidCA();
  /* equivalent to "C,C,C" */
  void SetTrustedServerCA();
  /* equivalent to "CT,CT,CT" */
  void SetTrustedCA();
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
  addTrust(&mTrust.sslFlags, ssl);
  addTrust(&mTrust.emailFlags, email);
  addTrust(&mTrust.objectSigningFlags, objsign);
}

nsNSSCertTrust::nsNSSCertTrust(CERTCertTrust *t)
{
  memcpy(&mTrust, t, sizeof(CERTCertTrust));
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
NS_IMPL_ISUPPORTS1(nsX509CertValidity, nsIX509CertValidity)

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

/* nsNSSCertificate */

NS_IMPL_THREADSAFE_ISUPPORTS1(nsNSSCertificate, nsIX509Cert)

nsNSSCertificate::nsNSSCertificate(char *certDER, int derLen)
{
  NS_INIT_ISUPPORTS();

  mCert = CERT_DecodeCertFromPackage(certDER, derLen);
}

nsNSSCertificate::nsNSSCertificate(CERTCertificate *cert)
{
  NS_INIT_ISUPPORTS();

  if (cert) 
    mCert = CERT_DupCertificate(cert);
}

nsNSSCertificate::~nsNSSCertificate()
{
  if (mCert)
    CERT_DestroyCertificate(mCert);
}

/* readonly attribute string dbKey; */
NS_IMETHODIMP 
nsNSSCertificate::GetDbKey(char * *aDbKey)
{
  SECStatus srv;
  SECItem key;

  NS_ENSURE_ARG(aDbKey);
  srv = CERT_KeyFromIssuerAndSN(mCert->arena, &mCert->derIssuer,
                                &mCert->serialNumber, &key);
  if (srv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  *aDbKey = NSSBase64_EncodeItem(nsnull, nsnull, 0, &key);
  return (*aDbKey) ? NS_OK : NS_ERROR_FAILURE;
}

/* readonly attribute string windowTitle; */
NS_IMETHODIMP 
nsNSSCertificate::GetWindowTitle(char * *aWindowTitle)
{
  if (mCert) {
    if (mCert->nickname) {
      *aWindowTitle = PL_strdup(mCert->nickname);
    } else {
      *aWindowTitle = CERT_GetCommonName(&mCert->subject);
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
  char *nickname = (mCert->nickname) ? mCert->nickname : "(no nickname)";
  nsAutoString nn = NS_ConvertASCIItoUCS2(nickname);
  *_nickname = nn.ToNewUnicode();
  return NS_OK;
}

/*  readonly attribute wstring emailAddress; */
NS_IMETHODIMP
nsNSSCertificate::GetEmailAddress(PRUnichar **_emailAddress)
{
  char *email = (mCert->emailAddr) ? mCert->emailAddr : "(no email address)";
  nsAutoString em = NS_ConvertASCIItoUCS2(email);
  *_emailAddress = em.ToNewUnicode();
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
      nsAutoString cn = NS_ConvertASCIItoUCS2(commonName);
      *aCommonName = cn.ToNewUnicode();
    } /*else {
      *aCommonName = NS_LITERAL_STRING("<not set>").get(), 
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
      nsAutoString org = NS_ConvertASCIItoUCS2(organization);
      *aOrganization = org.ToNewUnicode();
    } /*else {
      *aOrganization = NS_LITERAL_STRING("<not set>").get(), 
    }*/
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
      nsAutoString ou = NS_ConvertASCIItoUCS2(orgunit);
      *aOrganizationalUnit = ou.ToNewUnicode();
    } /*else {
      *aOrganizationalUnit = NS_LITERAL_STRING("<not set>").get(), 
    }*/
  }
  return NS_OK;
}

/* 
 * nsIEnumerator getChain(); 
 */
NS_IMETHODIMP
nsNSSCertificate::GetChain(nsIEnumerator **_rvChain)
{
  nsresult rv;
  CERTCertListNode *node;
  /* Get the cert chain from NSS */
  CERTCertList *nssChain;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting chain for \"%s\"\n", mCert->nickname));
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
  rv = array->Enumerate(_rvChain);
done:
  if (nssChain)
    CERT_DestroyCertList(nssChain);
  return rv;
}

/*  readonly attribute wstring subjectName; */
NS_IMETHODIMP
nsNSSCertificate::GetSubjectName(PRUnichar **_subjectName)
{
  if (mCert->subjectName) {
    nsAutoString sn = NS_ConvertASCIItoUCS2(mCert->subjectName);
    *_subjectName = sn.ToNewUnicode();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring issuerName; */
NS_IMETHODIMP
nsNSSCertificate::GetIssuerName(PRUnichar **_issuerName)
{
  if (mCert->issuerName) {
    nsAutoString in = NS_ConvertASCIItoUCS2(mCert->issuerName);
    *_issuerName = in.ToNewUnicode();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring serialNumber; */
NS_IMETHODIMP
nsNSSCertificate::GetSerialNumber(PRUnichar **_serialNumber)
{
  char *tmpstr = CERT_Hexify(&mCert->serialNumber, 1);
  if (tmpstr) {
    nsAutoString sn = NS_ConvertASCIItoUCS2(tmpstr);
    *_serialNumber = sn.ToNewUnicode();
    PR_Free(tmpstr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring rsaPubModulus; */
NS_IMETHODIMP
nsNSSCertificate::GetRsaPubModulus(PRUnichar **_rsaPubModulus)
{
  //char *tmpstr = CERT_Hexify(&mCert->serialNumber, 1);
  char *tmpstr = PL_strdup("not yet implemented");
  if (tmpstr) {
    nsAutoString rsap = NS_ConvertASCIItoUCS2(tmpstr);
    *_rsaPubModulus = rsap.ToNewUnicode();
    PR_Free(tmpstr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring sha1Fingerprint; */
NS_IMETHODIMP
nsNSSCertificate::GetSha1Fingerprint(PRUnichar **_sha1Fingerprint)
{
  unsigned char fingerprint[20];
  char *fpStr = NULL;
  SECItem fpItem;
  memset(fingerprint, 0, sizeof fingerprint);
  PK11_HashBuf(SEC_OID_SHA1, fingerprint, 
               mCert->derCert.data, mCert->derCert.len);
  fpItem.data = fingerprint;
  fpItem.len = SHA1_LENGTH;
  fpStr = CERT_Hexify(&fpItem, 1);
  if (fpStr) {
    nsAutoString sha1str = NS_ConvertASCIItoUCS2(fpStr);
    *_sha1Fingerprint = sha1str.ToNewUnicode();
    PR_Free(fpStr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/*  readonly attribute wstring md5Fingerprint; */
NS_IMETHODIMP
nsNSSCertificate::GetMd5Fingerprint(PRUnichar **_md5Fingerprint)
{
  unsigned char fingerprint[20];
  char *fpStr = NULL;
  SECItem fpItem;
  memset(fingerprint, 0, sizeof fingerprint);
  PK11_HashBuf(SEC_OID_MD5, fingerprint, 
               mCert->derCert.data, mCert->derCert.len);
  fpItem.data = fingerprint;
  fpItem.len = MD5_LENGTH;
  fpStr = CERT_Hexify(&fpItem, 1);
  if (fpStr) {
    nsAutoString md5str = NS_ConvertASCIItoUCS2(fpStr);
    *_md5Fingerprint = md5str.ToNewUnicode();
    PR_Free(fpStr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
  asciiname = NS_CONST_CAST(char*, NS_ConvertUCS2toUTF8(nickname).get());
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

/* nsIX509Cert getCertByKeyDB (in string dbkey); */
NS_IMETHODIMP 
nsNSSCertificateDB::GetCertByKeyDB(const char *dbkey, nsIX509Cert **_retval)
{
  SECItem keyItem = {siBuffer, nsnull, 0};
  SECItem *dummy;

  *_retval = nsnull; 
  dummy = NSSBase64_DecodeBuffer(nsnull, &keyItem, dbkey,
                                 (PRUint32)PL_strlen(dbkey)); 
  CERTCertificate *cert = CERT_FindCertByKey(CERT_GetDefaultCertDB(),
                                             &keyItem);
  PR_FREEIF(keyItem.data);
  if (cert) {
    nsNSSCertificate *nssCert = new nsNSSCertificate(cert);
    if (nssCert == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(nssCert);
    *_retval = NS_STATIC_CAST(nsIX509Cert*, nssCert);
  }
  return NS_OK;
}

/* [noscript] void getCertificateNames(in nsIPK11Token aToken,
 *                                     in unsigned long aType,
 *                                     in nsAutoStringRef rCertNameList);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::GetCertificateNames(nsIPK11Token *aToken, 
                                        PRUint32 aType,
                                        nsAutoString& rCertNameList)
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
  getCertNames(certList, aType, rCertNameList);
  rv = NS_OK;
  /*
   * finish up
   */
cleanup:
  if (certList)
    CERT_DestroyCertList(certList);
  return rv;
}

/*
 * [noscript] void importCertificate (in nsIX509Cert cert, 
 *                                    in unsigned long type,
 *                                    in unsigned long trust, 
 *                                    in wchar tokenName); 
 */
NS_IMETHODIMP 
nsNSSCertificateDB::ImportCertificate(nsIX509Cert *cert, 
                                      PRUint32 type,
                                      PRUint32 trusted,
                                      const PRUnichar *nickname)
{
  SECStatus srv = SECFailure;
  nsresult nsrv;
  CERTCertificate *tmpCert = NULL;
  nsNSSCertTrust trust;
  char *nick;
  SECItem der;
  switch (type) {
  case nsIX509Cert::CA_CERT:
    trust.SetValidCA();
    trust.AddCATrust(trusted & nsIX509Cert::TRUSTED_SSL,
                     trusted & nsIX509Cert::TRUSTED_EMAIL,
                     trusted & nsIX509Cert::TRUSTED_OBJSIGN);
    break;
  default:
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsrv = cert->GetRawDER((char **)&der.data, &der.len);
  if (nsrv != NS_OK)
    return NS_ERROR_FAILURE;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Creating temp cert\n"));
  tmpCert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &der,
                                    NULL, PR_FALSE, PR_TRUE);
  if (!tmpCert) goto done;
  if (nickname) {
    nick = NS_CONST_CAST(char*, NS_ConvertUCS2toUTF8(nickname).get());
  } else {
    nick = CERT_MakeCANickname(tmpCert);
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Created nick \"%s\"\n", nick));
  /* XXX check to see if cert is perm (it shouldn't be, but NSS asserts if
     it is */
  /* XXX this is an ugly peek into NSS */
  if (tmpCert->isperm) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Cert was already in db %s\n", nick));
    return NS_ERROR_FAILURE;
  }
  srv = CERT_AddTempCertToPerm(tmpCert, nick, trust.GetTrust());
done:
  if (tmpCert) 
    CERT_DestroyCertificate(tmpCert);
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
  if (type != nsIX509Cert::CA_CERT) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  ////// UGLY kluge until I decide how cert->GetCert() will work
  PRUnichar *wnn;
  cert->GetNickname(&wnn);
  char *nickname = PL_strdup(NS_ConvertUCS2toUTF8(wnn).get()); 
  CERTCertificate *nsscert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(),
                                                     nickname);
  if (!nsscert) {
    return NS_ERROR_FAILURE;
  }
  ////// end of kluge
  // always start with untrusted and move up
  trust.SetValidCA();
  trust.AddCATrust(trusted & nsIX509Cert::TRUSTED_SSL,
                   trusted & nsIX509Cert::TRUSTED_EMAIL,
                   trusted & nsIX509Cert::TRUSTED_OBJSIGN);
  srv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), 
                             nsscert,
                             trust.GetTrust());
  return (srv) ? NS_ERROR_FAILURE : NS_OK;
}

/*
 * boolean getCertTrust(in nsIX509Cert cert,
 *                      in unsigned long trustType);
 */
NS_IMETHODIMP 
nsNSSCertificateDB::GetCertTrust(nsIX509Cert *cert, 
                                 PRUint32 trustType,
                                 PRBool *_isTrusted)
{
  SECStatus srv;
  ////// UGLY kluge until I decide how cert->GetCert() will work
  PRUnichar *wnn;
  cert->GetNickname(&wnn);
  char *nickname = PL_strdup(NS_ConvertUCS2toUTF8(wnn).get()); 
  CERTCertificate *nsscert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(),
                                                     nickname);
  if (!nsscert) {
    return NS_ERROR_FAILURE;
  }
  ////// end of kluge
  CERTCertTrust nsstrust;
  srv = CERT_GetCertTrust(nsscert, &nsstrust);
  nsNSSCertTrust trust(&nsstrust);
  if (trustType & nsIX509Cert::TRUSTED_SSL) {
    *_isTrusted = trust.HasTrustedCA(PR_TRUE, PR_FALSE, PR_FALSE);
  } else if (trustType & nsIX509Cert::TRUSTED_EMAIL) {
    *_isTrusted = trust.HasTrustedCA(PR_FALSE, PR_TRUE, PR_FALSE);
  } else if (trustType & nsIX509Cert::TRUSTED_OBJSIGN) {
    *_isTrusted = trust.HasTrustedCA(PR_FALSE, PR_FALSE, PR_TRUE);
  } else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
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
                                     const PRUnichar **aCertNames)
{
  NS_ENSURE_ARG(aFile);
  nsPKCS12Blob blob;
  if (count == 0) return NS_OK;
  blob.SetToken(aToken);
  blob.LoadCerts(aCertNames, count);
  return blob.ExportToFile(aFile);
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
                                 nsString&     nameList)
{
  CERTCertListNode *node;
  
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("List of certs %d:\n", type));
  for (node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList);
       node = CERT_LIST_NEXT(node)) {
    if (getCertType(node->cert) == type) {
      nameList.AppendWithConversion(DELIM);
      if (type == nsIX509Cert::EMAIL_CERT) {
        nameList.AppendWithConversion(node->cert->emailAddr);
      } else {
        nameList.AppendWithConversion(node->cert->nickname);
      }
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("%s\n", node->cert->nickname));
    }
    if (type == nsIX509Cert::USER_CERT)
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("%s\n", node->cert->nickname));
  }
}

/* somewhat follows logic of cert_list_include_cert from PSM 1.x */
PRUint32 
nsNSSCertificateDB::getCertType(CERTCertificate *cert)
{
  char *nick = cert->nickname;
  char *email = cert->emailAddr;
  nsNSSCertTrust trust(cert->trust);
  if (nick) {
    if (trust.HasUser())
      return nsIX509Cert::USER_CERT;
    if (trust.HasCA())
      return nsIX509Cert::CA_CERT;
    if (trust.HasPeer(PR_TRUE, PR_FALSE, PR_FALSE))
      return nsIX509Cert::SERVER_CERT;
  }
  if (email && trust.HasPeer(PR_FALSE, PR_FALSE, PR_TRUE))
    return nsIX509Cert::EMAIL_CERT;
  return nsIX509Cert::UNKNOWN_CERT;
}


/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prmem.h"
#include "prerror.h"
#include "prprf.h"

#include "nsNSSComponent.h" // for PIPNSS string bundle calls.
#include "nsCOMPtr.h"
#include "nsArray.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertValidity.h"
#include "nsPKCS12Blob.h"
#include "nsPK11TokenDB.h"
#include "nsIX509Cert.h"
#include "nsISMimeCert.h"
#include "nsNSSASN1Object.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsILocaleService.h"
#include "nsIURI.h"
#include "nsTime.h"
#include "nsIProxyObjectManager.h"
#include "nsCRT.h"
#include "nsAutoLock.h"
#include "nsUsageArrayHelper.h"
#include "nsICertificateDialogs.h"
#include "nsNSSCertHelper.h"
#include "nsISupportsPrimitives.h"
#include "nsUnicharUtils.h"

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

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);


/* nsNSSCertificate */

NS_IMPL_THREADSAFE_ISUPPORTS2(nsNSSCertificate, nsIX509Cert,
                                                nsISMimeCert)

nsNSSCertificate*
nsNSSCertificate::ConstructFromDER(char *certDER, int derLen)
{
  nsNSSShutDownPreventionLock locker;

  if (!certDER || !derLen)
    return nsnull;

  CERTCertificate *aCert = CERT_DecodeCertFromPackage(certDER, derLen);
  
  if (!aCert)
    return nsnull;

  if(aCert->dbhandle == nsnull)
  {
    aCert->dbhandle = CERT_GetDefaultCertDB();
  }

  nsNSSCertificate *newObject = new nsNSSCertificate(aCert);
  CERT_DestroyCertificate(aCert);
  return newObject;
}

nsNSSCertificate::nsNSSCertificate(CERTCertificate *cert) : 
                                           mCert(nsnull),
                                           mPermDelete(PR_FALSE),
                                           mCertType(nsIX509Cert::UNKNOWN_CERT)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  if (cert) 
    mCert = CERT_DupCertificate(cert);
}

nsNSSCertificate::~nsNSSCertificate()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsNSSCertificate::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsNSSCertificate::destructorSafeDestroyNSSReference()
{
  if (isAlreadyShutDown())
    return;

  if (mPermDelete) {
    if (mCertType == nsNSSCertificate::USER_CERT) {
      nsCOMPtr<nsIInterfaceRequestor> cxt = new PipUIContext();
      PK11_DeleteTokenCertAndKey(mCert, cxt);
    } else if (!PK11_IsReadOnly(mCert->slot)) {
      // If the list of built-ins does contain a non-removable
      // copy of this certificate, our call will not remove
      // the certificate permanently, but rather remove all trust.
      SEC_DeletePermCertificate(mCert);
    }
  }

  if (mCert) {
    CERT_DestroyCertificate(mCert);
    mCert = nsnull;
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
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

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
  
  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));

  if (NS_FAILED(rv) || !nssComponent) {
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIX509Cert> x509Proxy;
  proxyman->GetProxyForObject( NS_UI_THREAD_EVENTQ,
                               nsIX509Cert::GetIID(),
                               NS_STATIC_CAST(nsIX509Cert*, this),
                               PROXY_SYNC | PROXY_ALWAYS,
                               getter_AddRefs(x509Proxy));

  if (!x509Proxy) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    rv = NS_OK;

    nsAutoString info;
    nsAutoString temp1;

    nickWithSerial.Append(nickname);

    if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoIssuedFor", info))) {
      details.Append(info);
      details.Append(PRUnichar(' '));
      if (NS_SUCCEEDED(x509Proxy->GetSubjectName(temp1)) && !temp1.IsEmpty()) {
        details.Append(temp1);
      }
      details.Append(PRUnichar('\n'));
    }

    if (NS_SUCCEEDED(x509Proxy->GetSerialNumber(temp1)) && !temp1.IsEmpty()) {
      details.AppendLiteral("  ");
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertDumpSerialNo", info))) {
        details.Append(info);
        details.AppendLiteral(": ");
      }
      details.Append(temp1);

      nickWithSerial.AppendLiteral(" [");
      nickWithSerial.Append(temp1);
      nickWithSerial.Append(PRUnichar(']'));

      details.Append(PRUnichar('\n'));
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
        details.AppendLiteral("  ");
        if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoValid", info))) {
          details.Append(info);
        }

        if (NS_SUCCEEDED(validity->GetNotBeforeLocalTime(temp1)) && !temp1.IsEmpty()) {
          details.Append(PRUnichar(' '));
          if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoFrom", info))) {
            details.Append(info);
            details.Append(PRUnichar(' '));
          }
          details.Append(temp1);
        }

        if (NS_SUCCEEDED(validity->GetNotAfterLocalTime(temp1)) && !temp1.IsEmpty()) {
          details.Append(PRUnichar(' '));
          if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoTo", info))) {
            details.Append(info);
            details.Append(PRUnichar(' '));
          }
          details.Append(temp1);
        }

        details.Append(PRUnichar('\n'));
      }
    }

    PRUint32 tempInt = 0;
    if (NS_SUCCEEDED(x509Proxy->GetUsagesString(PR_FALSE, &tempInt, temp1)) && !temp1.IsEmpty()) {
      details.AppendLiteral("  ");
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoPurposes", info))) {
        details.Append(info);
        details.AppendLiteral(": ");
      }
      details.Append(temp1);
      details.Append(PRUnichar('\n'));
    }

    if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoIssuedBy", info))) {
      details.Append(info);
      details.Append(PRUnichar(' '));

      if (NS_SUCCEEDED(x509Proxy->GetIssuerName(temp1)) && !temp1.IsEmpty()) {
        details.Append(temp1);
      }

      details.Append(PRUnichar('\n'));
    }

    if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoStoredIn", info))) {
      details.Append(info);
      details.Append(PRUnichar(' '));

      if (NS_SUCCEEDED(x509Proxy->GetTokenName(temp1)) && !temp1.IsEmpty()) {
        details.Append(temp1);
      }
    }

    /*
      the above produces output the following output:

      Issued to: $subjectName
        Serial number: $serialNumber
        Valid from: $starting_date to $expiration_date
        Purposes: $purposes
      Issued by: $issuerName
      Stored in: $token
    */
  }
  
  return rv;
}


/* readonly attribute string dbKey; */
NS_IMETHODIMP 
nsNSSCertificate::GetDbKey(char * *aDbKey)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  SECItem key;

  NS_ENSURE_ARG(aDbKey);
  *aDbKey = nsnull;
  key.len = NS_NSS_LONG*4+mCert->serialNumber.len+mCert->derIssuer.len;
  key.data = (unsigned char *)nsMemory::Alloc(key.len);
  NS_NSS_PUT_LONG(0,key.data); // later put moduleID
  NS_NSS_PUT_LONG(0,&key.data[NS_NSS_LONG]); // later put slotID
  NS_NSS_PUT_LONG(mCert->serialNumber.len,&key.data[NS_NSS_LONG*2]);
  NS_NSS_PUT_LONG(mCert->derIssuer.len,&key.data[NS_NSS_LONG*3]);
  memcpy(&key.data[NS_NSS_LONG*4], mCert->serialNumber.data,
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
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

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

NS_IMETHODIMP
nsNSSCertificate::GetNickname(nsAString &aNickname)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (mCert->nickname) {
    CopyUTF8toUTF16(mCert->nickname, aNickname);
  } else {
    nsresult rv;
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
    if (NS_FAILED(rv) || !nssComponent) {
      return NS_ERROR_FAILURE;
    }
    nssComponent->GetPIPNSSBundleString("CertNoNickname", aNickname);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetEmailAddress(nsAString &aEmailAddress)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (mCert->emailAddr) {
    CopyUTF8toUTF16(mCert->emailAddr, aEmailAddress);
  } else {
    nsresult rv;
    nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
    if (NS_FAILED(rv) || !nssComponent) {
      return NS_ERROR_FAILURE;
    }
    nssComponent->GetPIPNSSBundleString("CertNoEmailAddress", aEmailAddress);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetEmailAddresses(PRUint32 *aLength, PRUnichar*** aAddresses)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(aLength);
  NS_ENSURE_ARG(aAddresses);

  *aLength = 0;

  const char *aAddr;
  for (aAddr = CERT_GetFirstEmailAddress(mCert)
       ;
       aAddr
       ;
       aAddr = CERT_GetNextEmailAddress(mCert, aAddr))
  {
    ++(*aLength);
  }

  *aAddresses = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * (*aLength));
  if (!aAddresses)
    return NS_ERROR_OUT_OF_MEMORY;

  PRUint32 iAddr;
  for (aAddr = CERT_GetFirstEmailAddress(mCert), iAddr = 0
       ;
       aAddr
       ;
       aAddr = CERT_GetNextEmailAddress(mCert, aAddr), ++iAddr)
  {
    (*aAddresses)[iAddr] = ToNewUnicode(NS_ConvertUTF8toUCS2(aAddr));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::ContainsEmailAddress(const nsAString &aEmailAddress, PRBool *result)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(result);
  *result = PR_FALSE;

  const char *aAddr = nsnull;
  for (aAddr = CERT_GetFirstEmailAddress(mCert)
       ;
       aAddr
       ;
       aAddr = CERT_GetNextEmailAddress(mCert, aAddr))
  {
    NS_ConvertUTF8toUCS2 certAddr(aAddr);
    ToLowerCase(certAddr);

    nsAutoString testAddr(aEmailAddress);
    ToLowerCase(testAddr);
    
    if (certAddr == testAddr)
    {
      *result = PR_TRUE;
      break;
    }

  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetCommonName(nsAString &aCommonName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aCommonName.Truncate();
  if (mCert) {
    char *commonName = CERT_GetCommonName(&mCert->subject);
    if (commonName) {
      aCommonName = NS_ConvertUTF8toUCS2(commonName);
      PORT_Free(commonName);
    } /*else {
      *aCommonName = ToNewUnicode(NS_LITERAL_STRING("<not set>")), 
    }*/
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetOrganization(nsAString &aOrganization)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aOrganization.Truncate();
  if (mCert) {
    char *organization = CERT_GetOrgName(&mCert->subject);
    if (organization) {
      aOrganization = NS_ConvertUTF8toUCS2(organization);
      PORT_Free(organization);
    } /*else {
      *aOrganization = ToNewUnicode(NS_LITERAL_STRING("<not set>")), 
    }*/
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerCommonName(nsAString &aCommonName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aCommonName.Truncate();
  if (mCert) {
    char *commonName = CERT_GetCommonName(&mCert->issuer);
    if (commonName) {
      aCommonName = NS_ConvertUTF8toUCS2(commonName);
      PORT_Free(commonName);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerOrganization(nsAString &aOrganization)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aOrganization.Truncate();
  if (mCert) {
    char *organization = CERT_GetOrgName(&mCert->issuer);
    if (organization) {
      aOrganization = NS_ConvertUTF8toUCS2(organization);
      PORT_Free(organization);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerOrganizationUnit(nsAString &aOrganizationUnit)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aOrganizationUnit.Truncate();
  if (mCert) {
    char *organizationUnit = CERT_GetOrgUnitName(&mCert->issuer);
    if (organizationUnit) {
      aOrganizationUnit = NS_ConvertUTF8toUCS2(organizationUnit);
      PORT_Free(organizationUnit);
    }
  }
  return NS_OK;
}

/* readonly attribute nsIX509Cert issuer; */
NS_IMETHODIMP 
nsNSSCertificate::GetIssuer(nsIX509Cert * *aIssuer)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

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
nsNSSCertificate::GetOrganizationalUnit(nsAString &aOrganizationalUnit)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aOrganizationalUnit.Truncate();
  if (mCert) {
    char *orgunit = CERT_GetOrgUnitName(&mCert->subject);
    if (orgunit) {
      aOrganizationalUnit = NS_ConvertUTF8toUCS2(orgunit);
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
nsNSSCertificate::GetChain(nsIArray **_rvChain)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

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
  nsCOMPtr<nsIMutableArray> array;
  rv = NS_NewArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) { 
    goto done; 
  }
  for (node = CERT_LIST_HEAD(nssChain);
       !CERT_LIST_END(node, nssChain);
       node = CERT_LIST_NEXT(node)) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("adding %s to chain\n", node->cert->nickname));
    nsCOMPtr<nsIX509Cert> cert = new nsNSSCertificate(node->cert);
    array->AppendElement(cert, PR_FALSE);
  }
#else // workaround here
  CERTCertificate *cert = nsnull;
  /* enumerate the chain for scripting purposes */
  nsCOMPtr<nsIMutableArray> array;
  rv = NS_NewArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) { 
    goto done; 
  }
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting chain for \"%s\"\n", mCert->nickname));
  cert = CERT_DupCertificate(mCert);
  while (cert) {
    nsCOMPtr<nsIX509Cert> pipCert = new nsNSSCertificate(cert);
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("adding %s to chain\n", cert->nickname));
    array->AppendElement(pipCert, PR_FALSE);
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

NS_IMETHODIMP
nsNSSCertificate::GetSubjectName(nsAString &_subjectName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  _subjectName.Truncate();
  if (mCert->subjectName) {
    _subjectName = NS_ConvertUTF8toUCS2(mCert->subjectName);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::GetIssuerName(nsAString &_issuerName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  _issuerName.Truncate();
  if (mCert->issuerName) {
    _issuerName = NS_ConvertUTF8toUCS2(mCert->issuerName);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::GetSerialNumber(nsAString &_serialNumber)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  _serialNumber.Truncate();
  nsXPIDLCString tmpstr; tmpstr.Adopt(CERT_Hexify(&mCert->serialNumber, 1));
  if (tmpstr.get()) {
    _serialNumber = NS_ConvertASCIItoUCS2(tmpstr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::GetSha1Fingerprint(nsAString &_sha1Fingerprint)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  _sha1Fingerprint.Truncate();
  unsigned char fingerprint[20];
  SECItem fpItem;
  memset(fingerprint, 0, sizeof fingerprint);
  PK11_HashBuf(SEC_OID_SHA1, fingerprint, 
               mCert->derCert.data, mCert->derCert.len);
  fpItem.data = fingerprint;
  fpItem.len = SHA1_LENGTH;
  nsXPIDLCString fpStr; fpStr.Adopt(CERT_Hexify(&fpItem, 1));
  if (fpStr.get()) {
    _sha1Fingerprint = NS_ConvertASCIItoUCS2(fpStr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::GetMd5Fingerprint(nsAString &_md5Fingerprint)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  _md5Fingerprint.Truncate();
  unsigned char fingerprint[20];
  SECItem fpItem;
  memset(fingerprint, 0, sizeof fingerprint);
  PK11_HashBuf(SEC_OID_MD5, fingerprint, 
               mCert->derCert.data, mCert->derCert.len);
  fpItem.data = fingerprint;
  fpItem.len = MD5_LENGTH;
  nsXPIDLCString fpStr; fpStr.Adopt(CERT_Hexify(&fpItem, 1));
  if (fpStr.get()) {
    _md5Fingerprint = NS_ConvertASCIItoUCS2(fpStr);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::GetTokenName(nsAString &aTokenName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  aTokenName.Truncate();
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
        aTokenName = NS_ConvertUTF8toUCS2(token);
      }
    } else {
      nsresult rv;
      nsAutoString tok;
      nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));
      if (NS_FAILED(rv)) return rv;
      rv = nssComponent->GetPIPNSSBundleString("InternalToken", tok);
      if (NS_SUCCEEDED(rv))
        aTokenName = tok;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetRawDER(PRUint32 *aLength, PRUint8 **aArray)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (mCert) {
    *aArray = (PRUint8 *)mCert->derCert.data;
    *aLength = mCert->derCert.len;
    return NS_OK;
  }
  *aLength = 0;
  return NS_ERROR_FAILURE;
}

CERTCertificate *
nsNSSCertificate::GetCert()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return nsnull;

  return (mCert) ? CERT_DupCertificate(mCert) : nsnull;
}

NS_IMETHODIMP
nsNSSCertificate::GetValidity(nsIX509CertValidity **aValidity)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

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
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

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


NS_IMETHODIMP
nsNSSCertificate::GetUsagesArray(PRBool ignoreOcsp,
                                 PRUint32 *_verified,
                                 PRUint32 *_count,
                                 PRUnichar ***_usages)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv;
  const int max_usages = 13;
  PRUnichar *tmpUsages[max_usages];
  const char *suffix = "";
  PRUint32 tmpCount;
  nsUsageArrayHelper uah(mCert);
  rv = uah.GetUsagesArray(suffix, ignoreOcsp, max_usages, _verified, &tmpCount, tmpUsages);
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

NS_IMETHODIMP
nsNSSCertificate::GetUsagesString(PRBool ignoreOcsp,
                                  PRUint32   *_verified,
                                  nsAString &_usages)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv;
  const int max_usages = 13;
  PRUnichar *tmpUsages[max_usages];
  const char *suffix = "_p";
  PRUint32 tmpCount;
  nsUsageArrayHelper uah(mCert);
  rv = uah.GetUsagesArray(suffix, ignoreOcsp, max_usages, _verified, &tmpCount, tmpUsages);
  _usages.Truncate();
  for (PRUint32 i=0; i<tmpCount; i++) {
    if (i>0) _usages.AppendLiteral(",");
    _usages.Append(tmpUsages[i]);
    nsMemory::Free(tmpUsages[i]);
  }
  return NS_OK;
}

#if defined(DEBUG_javi) || defined(DEBUG_jgmyers)
void
DumpASN1Object(nsIASN1Object *object, unsigned int level)
{
  nsAutoString dispNameU, dispValU;
  unsigned int i;
  nsCOMPtr<nsIMutableArray> asn1Objects;
  nsCOMPtr<nsISupports> isupports;
  nsCOMPtr<nsIASN1Object> currObject;
  PRBool processObjects;
  PRUint32 numObjects;

  for (i=0; i<level; i++)
    printf ("  ");

  object->GetDisplayName(dispNameU);
  nsCOMPtr<nsIASN1Sequence> sequence(do_QueryInterface(object));
  if (sequence) {
    printf ("%s ", NS_ConvertUCS2toUTF8(dispNameU).get());
    sequence->GetIsValidContainer(&processObjects);
    if (processObjects) {
      printf("\n");
      sequence->GetASN1Objects(getter_AddRefs(asn1Objects));
      asn1Objects->GetLength(&numObjects);
      for (i=0; i<numObjects;i++) {
        asn1Objects->QueryElementAt(i, NS_GET_IID(nsISupports), getter_AddRefs(currObject));
        DumpASN1Object(currObject, level+1);    
      }
    } else { 
      object->GetDisplayValue(dispValU);
      printf("= %s\n", NS_ConvertUCS2toUTF8(dispValU).get()); 
    }
  } else { 
    object->GetDisplayValue(dispValU);
    printf("%s = %s\n",NS_ConvertUCS2toUTF8(dispNameU).get(), 
                       NS_ConvertUCS2toUTF8(dispValU).get()); 
  }
}
#endif

/* readonly attribute nsIASN1Object ASN1Structure; */
NS_IMETHODIMP 
nsNSSCertificate::GetASN1Structure(nsIASN1Object * *aASN1Structure)
{
  nsNSSShutDownPreventionLock locker;
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
nsNSSCertificate::Equals(nsIX509Cert *other, PRBool *result)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

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
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (SECSuccess != CERT_SaveSMimeProfile(mCert, nsnull, nsnull))
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}


char* nsNSSCertificate::defaultServerNickname(CERTCertificate* cert)
{
  nsNSSShutDownPreventionLock locker;
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


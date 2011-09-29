/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Kai Engert <kengert@redhat.com>
 *   Jesper Kristensen <mail@jesperkristensen.dk>
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
#include "nsNSSCleaner.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertValidity.h"
#include "nsPKCS12Blob.h"
#include "nsPK11TokenDB.h"
#include "nsIX509Cert.h"
#include "nsIX509Cert3.h"
#include "nsISMimeCert.h"
#include "nsNSSASN1Object.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIURI.h"
#include "nsIProxyObjectManager.h"
#include "nsCRT.h"
#include "nsUsageArrayHelper.h"
#include "nsICertificateDialogs.h"
#include "nsNSSCertHelper.h"
#include "nsISupportsPrimitives.h"
#include "nsUnicharUtils.h"
#include "nsThreadUtils.h"
#include "nsCertVerificationThread.h"
#include "nsIObjectOutputStream.h"
#include "nsIObjectInputStream.h"
#include "nsIProgrammingLanguage.h"

#include "nsXULAppAPI.h"

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
#include "cms.h"
#include "cert.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

NSSCleanupAutoPtrClass(CERTCertificateList, CERT_DestroyCertificateList)
NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)
NSSCleanupAutoPtrClass(NSSCMSMessage, NSS_CMSMessage_Destroy)
NSSCleanupAutoPtrClass_WithParam(PLArenaPool, PORT_FreeArena, FalseParam, PR_FALSE)
NSSCleanupAutoPtrClass(NSSCMSSignedData, NSS_CMSSignedData_Destroy)
NSSCleanupAutoPtrClass(PK11SlotList, PK11_FreeSlotList)

// This is being stored in an PRUint32 that can otherwise
// only take values from nsIX509Cert's list of cert types.
// As nsIX509Cert is frozen, we choose a value not contained
// in the list to mean not yet initialized.
#define CERT_TYPE_NOT_YET_INITIALIZED (1 << 30)

/* nsNSSCertificate */

NS_IMPL_THREADSAFE_ISUPPORTS7(nsNSSCertificate, nsIX509Cert,
                                                nsIX509Cert2,
                                                nsIX509Cert3,
                                                nsIIdentityInfo,
                                                nsISMimeCert,
                                                nsISerializable,
                                                nsIClassInfo)

/* static */
nsNSSCertificate*
nsNSSCertificate::Create(CERTCertificate *cert)
{
  if (GeckoProcessType_Default != XRE_GetProcessType()) {
    NS_ERROR("Trying to initialize nsNSSCertificate in a non-chrome process!");
    return nsnull;
  }
  if (cert)
    return new nsNSSCertificate(cert);
  else
    return new nsNSSCertificate();
}

nsNSSCertificate*
nsNSSCertificate::ConstructFromDER(char *certDER, int derLen)
{
  // On non-chrome process prevent instantiation
  if (GeckoProcessType_Default != XRE_GetProcessType())
    return nsnull;

  nsNSSCertificate* newObject = nsNSSCertificate::Create();
  if (newObject && !newObject->InitFromDER(certDER, derLen)) {
    delete newObject;
    newObject = nsnull;
  }

  return newObject;
}

bool
nsNSSCertificate::InitFromDER(char *certDER, int derLen)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return PR_FALSE;

  if (!certDER || !derLen)
    return PR_FALSE;

  CERTCertificate *aCert = CERT_DecodeCertFromPackage(certDER, derLen);
  
  if (!aCert)
    return PR_FALSE;

  if(aCert->dbhandle == nsnull)
  {
    aCert->dbhandle = CERT_GetDefaultCertDB();
  }

  mCert = aCert;
  return PR_TRUE;
}

nsNSSCertificate::nsNSSCertificate(CERTCertificate *cert) : 
                                           mCert(nsnull),
                                           mPermDelete(PR_FALSE),
                                           mCertType(CERT_TYPE_NOT_YET_INITIALIZED),
                                           mCachedEVStatus(ev_status_unknown)
{
#if defined(DEBUG)
  if (GeckoProcessType_Default != XRE_GetProcessType())
    NS_ERROR("Trying to initialize nsNSSCertificate in a non-chrome process!");
#endif

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return;

  if (cert) 
    mCert = CERT_DupCertificate(cert);
}

nsNSSCertificate::nsNSSCertificate() : 
  mCert(nsnull),
  mPermDelete(PR_FALSE),
  mCertType(CERT_TYPE_NOT_YET_INITIALIZED),
  mCachedEVStatus(ev_status_unknown)
{
  if (GeckoProcessType_Default != XRE_GetProcessType())
    NS_ERROR("Trying to initialize nsNSSCertificate in a non-chrome process!");
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
nsNSSCertificate::GetCertType(PRUint32 *aCertType)
{
  if (mCertType == CERT_TYPE_NOT_YET_INITIALIZED) {
     // only determine cert type once and cache it
     mCertType = getCertType(mCert);
  }
  *aCertType = mCertType;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIsSelfSigned(bool *aIsSelfSigned)
{
  NS_ENSURE_ARG(aIsSelfSigned);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  *aIsSelfSigned = mCert->isRoot;
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
GetKeyUsagesString(CERTCertificate *cert, nsINSSComponent *nssComponent, 
                   nsString &text)
{
  text.Truncate();

  SECItem keyUsageItem;
  keyUsageItem.data = NULL;

  SECStatus srv;

  /* There is no extension, v1 or v2 certificate */
  if (!cert->extensions)
    return NS_OK;


  srv = CERT_FindKeyUsageExtension(cert, &keyUsageItem);
  if (srv == SECFailure) {
    if (PORT_GetError () == SEC_ERROR_EXTENSION_NOT_FOUND)
      return NS_OK;
    else
      return NS_ERROR_FAILURE;
  }

  unsigned char keyUsage = keyUsageItem.data[0];
  nsAutoString local;
  nsresult rv;
  const PRUnichar *comma = NS_LITERAL_STRING(",").get();

  if (keyUsage & KU_DIGITAL_SIGNATURE) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUSign", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_NON_REPUDIATION) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUNonRep", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_KEY_ENCIPHERMENT) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUEnc", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_DATA_ENCIPHERMENT) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUDEnc", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_KEY_AGREEMENT) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUKA", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_KEY_CERT_SIGN) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUCertSign", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }
  if (keyUsage & KU_CRL_SIGN) {
    rv = nssComponent->GetPIPNSSBundleString("CertDumpKUCRLSign", local);
    if (NS_SUCCEEDED(rv)) {
      if (!text.IsEmpty()) text.Append(comma);
      text.Append(local.get());
    }
  }

  PORT_Free (keyUsageItem.data);
  return NS_OK;
}

nsresult
nsNSSCertificate::FormatUIStrings(const nsAutoString &nickname, nsAutoString &nickWithSerial, nsAutoString &details)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsINSSComponent> nssComponent(do_GetService(kNSSComponentCID, &rv));

  if (NS_FAILED(rv) || !nssComponent) {
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIX509Cert> x509Proxy;
  NS_GetProxyForObject( NS_PROXY_TO_MAIN_THREAD,
                        NS_GET_IID(nsIX509Cert),
                        static_cast<nsIX509Cert*>(this),
                        NS_PROXY_SYNC | NS_PROXY_ALWAYS,
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
        NS_GetProxyForObject( NS_PROXY_TO_MAIN_THREAD,
                              NS_GET_IID(nsIX509CertValidity),
                              originalValidity,
                              NS_PROXY_SYNC | NS_PROXY_ALWAYS,
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

    if (NS_SUCCEEDED(GetKeyUsagesString(mCert, nssComponent, temp1)) && !temp1.IsEmpty()) {
      details.AppendLiteral("  ");
      if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertDumpKeyUsage", info))) {
        details.Append(info);
        details.AppendLiteral(": ");
      }
      details.Append(temp1);
      details.Append(PRUnichar('\n'));
    }

    nsAutoString firstEmail;
    const char *aWalkAddr;
    for (aWalkAddr = CERT_GetFirstEmailAddress(mCert)
         ;
         aWalkAddr
         ;
         aWalkAddr = CERT_GetNextEmailAddress(mCert, aWalkAddr))
    {
      NS_ConvertUTF8toUTF16 email(aWalkAddr);
      if (email.IsEmpty())
        continue;

      if (firstEmail.IsEmpty()) {
        /*
         * If the first email address from the subject DN is also present
         * in the subjectAltName extension, GetEmailAddresses() will return
         * it twice (as received from NSS). Remember the first address so that
         * we can filter out duplicates later on.
         */
        firstEmail = email;

        details.AppendLiteral("  ");
        if (NS_SUCCEEDED(nssComponent->GetPIPNSSBundleString("CertInfoEmail", info))) {
          details.Append(info);
          details.AppendLiteral(": ");
        }
        details.Append(email);
      }
      else {
        // Append current address if it's different from the first one.
        if (!firstEmail.Equals(email)) {
          details.AppendLiteral(", ");
          details.Append(email);
        }
      }
    }

    if (!firstEmail.IsEmpty()) {
      // We got at least one email address, so we want a newline
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
      the above produces the following output:

      Issued to: $subjectName
        Serial number: $serialNumber
        Valid from: $starting_date to $expiration_date
        Purposes: $purposes
        Certificate Key usage: $usages
        Email: $address(es)
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
  if (!key.data)
    return NS_ERROR_OUT_OF_MEMORY;
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
        if (mCert->subjectName) {
          *aWindowTitle = PL_strdup(mCert->subjectName);
        } else if (mCert->emailAddr) {
          *aWindowTitle = PL_strdup(mCert->emailAddr);
        } else {
          *aWindowTitle = PL_strdup("");
        }
      }
    }
  } else {
    NS_ERROR("Somehow got nsnull for mCertificate in nsNSSCertificate.");
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
  if (!*aAddresses)
    return NS_ERROR_OUT_OF_MEMORY;

  PRUint32 iAddr;
  for (aAddr = CERT_GetFirstEmailAddress(mCert), iAddr = 0
       ;
       aAddr
       ;
       aAddr = CERT_GetNextEmailAddress(mCert, aAddr), ++iAddr)
  {
    (*aAddresses)[iAddr] = ToNewUnicode(NS_ConvertUTF8toUTF16(aAddr));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::ContainsEmailAddress(const nsAString &aEmailAddress, bool *result)
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
    NS_ConvertUTF8toUTF16 certAddr(aAddr);
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
      aCommonName = NS_ConvertUTF8toUTF16(commonName);
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
      aOrganization = NS_ConvertUTF8toUTF16(organization);
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
      aCommonName = NS_ConvertUTF8toUTF16(commonName);
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
      aOrganization = NS_ConvertUTF8toUTF16(organization);
      PORT_Free(organization);
    } else {
      return GetIssuerCommonName(aOrganization);
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
      aOrganizationUnit = NS_ConvertUTF8toUTF16(organizationUnit);
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
    nsCOMPtr<nsIX509Cert> cert = nsNSSCertificate::Create(issuer);
    if (cert) {
      *aIssuer = cert;
      NS_ADDREF(*aIssuer);
    }
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
      aOrganizationalUnit = NS_ConvertUTF8toUTF16(orgunit);
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
  nssChain = CERT_GetCertChainFromCert(mCert, PR_Now(), certUsageSSLClient);
  if (!nssChain)
    return NS_ERROR_FAILURE;
  /* enumerate the chain for scripting purposes */
  nsCOMPtr<nsIMutableArray> array =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_FAILED(rv)) { 
    goto done; 
  }
  CERTCertListNode *node;
  for (node = CERT_LIST_HEAD(nssChain);
       !CERT_LIST_END(node, nssChain);
       node = CERT_LIST_NEXT(node)) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("adding %s to chain\n", node->cert->nickname));
    nsCOMPtr<nsIX509Cert> cert = nsNSSCertificate::Create(node->cert);
    array->AppendElement(cert, PR_FALSE);
  }
  *_rvChain = array;
  NS_IF_ADDREF(*_rvChain);
  rv = NS_OK;
done:
  if (nssChain)
    CERT_DestroyCertList(nssChain);
  return rv;
}

NS_IMETHODIMP
nsNSSCertificate::GetAllTokenNames(PRUint32 *aLength, PRUnichar*** aTokenNames)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(aLength);
  NS_ENSURE_ARG(aTokenNames);
  *aLength = 0;
  *aTokenNames = NULL;

  /* Get the slots from NSS */
  PK11SlotList *slots = NULL;
  PK11SlotListCleaner slotCleaner(slots);
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("Getting slots for \"%s\"\n", mCert->nickname));
  slots = PK11_GetAllSlotsForCert(mCert, NULL);
  if (!slots) {
    if (PORT_GetError() == SEC_ERROR_NO_TOKEN)
      return NS_OK; // List of slots is empty, return empty array
    else
      return NS_ERROR_FAILURE;
  }

  /* read the token names from slots */
  PK11SlotListElement *le;

  for (le = slots->head; le; le = le->next) {
    ++(*aLength);
  }

  *aTokenNames = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * (*aLength));
  if (!*aTokenNames) {
    *aLength = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUint32 iToken;
  for (le = slots->head, iToken = 0; le; le = le->next, ++iToken) {
    char *token = PK11_GetTokenName(le->slot);
    (*aTokenNames)[iToken] = ToNewUnicode(NS_ConvertUTF8toUTF16(token));
    if (!(*aTokenNames)[iToken]) {
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(iToken, *aTokenNames);
      *aLength = 0;
      *aTokenNames = NULL;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetSubjectName(nsAString &_subjectName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  _subjectName.Truncate();
  if (mCert->subjectName) {
    _subjectName = NS_ConvertUTF8toUTF16(mCert->subjectName);
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
    _issuerName = NS_ConvertUTF8toUTF16(mCert->issuerName);
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
  char *tmpstr = CERT_Hexify(&mCert->serialNumber, 1);
  if (tmpstr) {
    _serialNumber = NS_ConvertASCIItoUTF16(tmpstr);
    PORT_Free(tmpstr);
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
  char *fpStr = CERT_Hexify(&fpItem, 1);
  if (fpStr) {
    _sha1Fingerprint = NS_ConvertASCIItoUTF16(fpStr);
    PORT_Free(fpStr);
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
  char *fpStr = CERT_Hexify(&fpItem, 1);
  if (fpStr) {
    _md5Fingerprint = NS_ConvertASCIItoUTF16(fpStr);
    PORT_Free(fpStr);
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
        aTokenName = NS_ConvertUTF8toUTF16(token);
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
    *aArray = (PRUint8*)nsMemory::Alloc(mCert->derCert.len);
    if (*aArray) {
      memcpy(*aArray, mCert->derCert.data, mCert->derCert.len);
      *aLength = mCert->derCert.len;
      return NS_OK;
    }
  }
  *aLength = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNSSCertificate::ExportAsCMS(PRUint32 chainMode,
                              PRUint32 *aLength, PRUint8 **aArray)
{
  NS_ENSURE_ARG(aLength);
  NS_ENSURE_ARG(aArray);

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (!mCert)
    return NS_ERROR_FAILURE;

  switch (chainMode) {
    case nsIX509Cert3::CMS_CHAIN_MODE_CertOnly:
    case nsIX509Cert3::CMS_CHAIN_MODE_CertChain:
    case nsIX509Cert3::CMS_CHAIN_MODE_CertChainWithRoot:
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  };

  PLArenaPool *arena = PORT_NewArena(1024);
  PLArenaPoolCleanerFalseParam arenaCleaner(arena);
  if (!arena) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - out of memory\n"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NSSCMSMessage *cmsg = NSS_CMSMessage_Create(nsnull);
  NSSCMSMessageCleaner cmsgCleaner(cmsg);
  if (!cmsg) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - can't create CMS message\n"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  /*
   * first, create SignedData with the certificate only (no chain)
   */
  NSSCMSSignedData *sigd = NSS_CMSSignedData_CreateCertsOnly(cmsg, mCert, PR_FALSE);
  NSSCMSSignedDataCleaner sigdCleaner(sigd);
  if (!sigd) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - can't create SignedData\n"));
    return NS_ERROR_FAILURE;
  }

  /*
   * Calling NSS_CMSSignedData_CreateCertsOnly() will not allow us
   * to specify the inclusion of the root, but CERT_CertChainFromCert() does.
   * Since CERT_CertChainFromCert() also includes the certificate itself,
   * we have to start at the issuing cert (to avoid duplicate certs
   * in the SignedData).
   */
  if (chainMode == nsIX509Cert3::CMS_CHAIN_MODE_CertChain ||
      chainMode == nsIX509Cert3::CMS_CHAIN_MODE_CertChainWithRoot) {
    CERTCertificate *issuerCert = CERT_FindCertIssuer(mCert, PR_Now(), certUsageAnyCA);
    CERTCertificateCleaner issuerCertCleaner(issuerCert);
    /*
     * the issuerCert of a self signed root is the cert itself,
     * so make sure we're not adding duplicates, again
     */
    if (issuerCert && issuerCert != mCert) {
      bool includeRoot = 
        (chainMode == nsIX509Cert3::CMS_CHAIN_MODE_CertChainWithRoot);
      CERTCertificateList *certChain = CERT_CertChainFromCert(issuerCert, certUsageAnyCA, includeRoot);
      CERTCertificateListCleaner certChainCleaner(certChain);
      if (certChain) {
        if (NSS_CMSSignedData_AddCertList(sigd, certChain) == SECSuccess) {
          certChainCleaner.detach();
        }
        else {
          PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
                 ("nsNSSCertificate::ExportAsCMS - can't add chain\n"));
          return NS_ERROR_FAILURE;
        }
      }
      else { 
        /* try to add the issuerCert, at least */
        if (NSS_CMSSignedData_AddCertificate(sigd, issuerCert)
            == SECSuccess) {
          issuerCertCleaner.detach();
        }
        else {
          PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
                 ("nsNSSCertificate::ExportAsCMS - can't add issuer cert\n"));
          return NS_ERROR_FAILURE;
        }
      }
    }
  }

  NSSCMSContentInfo *cinfo = NSS_CMSMessage_GetContentInfo(cmsg);
  if (NSS_CMSContentInfo_SetContent_SignedData(cmsg, cinfo, sigd)
       == SECSuccess) {
    sigdCleaner.detach();
  }
  else {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - can't attach SignedData\n"));
    return NS_ERROR_FAILURE;
  }

  SECItem certP7 = { siBuffer, nsnull, 0 };
  NSSCMSEncoderContext *ecx = NSS_CMSEncoder_Start(cmsg, nsnull, nsnull, &certP7, arena,
                                                   nsnull, nsnull, nsnull, nsnull, nsnull,
                                                   nsnull);
  if (!ecx) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - can't create encoder context\n"));
    return NS_ERROR_FAILURE;
  }

  if (NSS_CMSEncoder_Finish(ecx) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("nsNSSCertificate::ExportAsCMS - failed to add encoded data\n"));
    return NS_ERROR_FAILURE;
  }

  *aArray = (PRUint8*)nsMemory::Alloc(certP7.len);
  if (!*aArray)
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy(*aArray, certP7.data, certP7.len);
  *aLength = certP7.len;
  return NS_OK;
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
  *aValidity = static_cast<nsIX509CertValidity*>(validity);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::VerifyForUsage(PRUint32 usage, PRUint32 *verificationResult)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(verificationResult);

  nsresult nsrv;
  nsCOMPtr<nsINSSComponent> inss = do_GetService(kNSSComponentCID, &nsrv);
  if (!inss)
    return nsrv;
  nsRefPtr<nsCERTValInParamWrapper> survivingParams;
  nsrv = inss->GetDefaultCERTValInParam(survivingParams);
  if (NS_FAILED(nsrv))
    return nsrv;
  
  SECCertificateUsage nss_usage;
  
  switch (usage)
  {
    case CERT_USAGE_SSLClient:
      nss_usage = certificateUsageSSLClient;
      break;

    case CERT_USAGE_SSLServer:
      nss_usage = certificateUsageSSLServer;
      break;

    case CERT_USAGE_SSLServerWithStepUp:
      nss_usage = certificateUsageSSLServerWithStepUp;
      break;

    case CERT_USAGE_SSLCA:
      nss_usage = certificateUsageSSLCA;
      break;

    case CERT_USAGE_EmailSigner:
      nss_usage = certificateUsageEmailSigner;
      break;

    case CERT_USAGE_EmailRecipient:
      nss_usage = certificateUsageEmailRecipient;
      break;

    case CERT_USAGE_ObjectSigner:
      nss_usage = certificateUsageObjectSigner;
      break;

    case CERT_USAGE_UserCertImport:
      nss_usage = certificateUsageUserCertImport;
      break;

    case CERT_USAGE_VerifyCA:
      nss_usage = certificateUsageVerifyCA;
      break;

    case CERT_USAGE_ProtectedObjectSigner:
      nss_usage = certificateUsageProtectedObjectSigner;
      break;

    case CERT_USAGE_StatusResponder:
      nss_usage = certificateUsageStatusResponder;
      break;

    case CERT_USAGE_AnyCA:
      nss_usage = certificateUsageAnyCA;
      break;

    default:
      return NS_ERROR_FAILURE;
  }

  SECStatus verify_result;
  if (!nsNSSComponent::globalConstFlagUsePKIXVerification) {
    CERTCertDBHandle *defaultcertdb = CERT_GetDefaultCertDB();
    verify_result = CERT_VerifyCertificateNow(defaultcertdb, mCert, PR_TRUE, 
                                              nss_usage, NULL, NULL);
  }
  else {
    CERTValOutParam cvout[1];
    cvout[0].type = cert_po_end;
    verify_result = CERT_PKIXVerifyCert(mCert, nss_usage,
                                        survivingParams->GetRawPointerForNSS(),
                                        cvout, NULL);
  }
  
  if (verify_result == SECSuccess)
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
nsNSSCertificate::GetUsagesArray(bool localOnly,
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
  rv = uah.GetUsagesArray(suffix, localOnly, max_usages, _verified, &tmpCount, tmpUsages);
  NS_ENSURE_SUCCESS(rv,rv);
  if (tmpCount > 0) {
    *_usages = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *) * tmpCount);
    if (!*_usages)
      return NS_ERROR_OUT_OF_MEMORY;
    for (PRUint32 i=0; i<tmpCount; i++) {
      (*_usages)[i] = tmpUsages[i];
    }
    *_count = tmpCount;
    return NS_OK;
  }
  *_usages = (PRUnichar **)nsMemory::Alloc(sizeof(PRUnichar *));
  if (!*_usages)
    return NS_ERROR_OUT_OF_MEMORY;
  *_count = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::RequestUsagesArrayAsync(nsICertVerificationListener *aResultListener)
{
  if (!aResultListener)
    return NS_ERROR_FAILURE;
  
  nsCertVerificationJob *job = new nsCertVerificationJob;
  if (!job)
    return NS_ERROR_OUT_OF_MEMORY;

  job->mCert = this;
  job->mListener = aResultListener;

  nsresult rv = nsCertVerificationThread::addJob(job);
  if (NS_FAILED(rv))
    delete job;

  return rv;
}

NS_IMETHODIMP
nsNSSCertificate::GetUsagesString(bool localOnly,
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
  rv = uah.GetUsagesArray(suffix, localOnly, max_usages, _verified, &tmpCount, tmpUsages);
  NS_ENSURE_SUCCESS(rv,rv);
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
  bool processObjects;
  PRUint32 numObjects;

  for (i=0; i<level; i++)
    printf ("  ");

  object->GetDisplayName(dispNameU);
  nsCOMPtr<nsIASN1Sequence> sequence(do_QueryInterface(object));
  if (sequence) {
    printf ("%s ", NS_ConvertUTF16toUTF8(dispNameU).get());
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
      printf("= %s\n", NS_ConvertUTF16toUTF8(dispValU).get()); 
    }
  } else { 
    object->GetDisplayValue(dispValU);
    printf("%s = %s\n",NS_ConvertUTF16toUTF8(dispNameU).get(), 
                       NS_ConvertUTF16toUTF8(dispValU).get()); 
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
nsNSSCertificate::Equals(nsIX509Cert *other, bool *result)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(other);
  NS_ENSURE_ARG(result);

  nsCOMPtr<nsIX509Cert2> other2 = do_QueryInterface(other);
  if (!other2)
    return NS_ERROR_FAILURE;
 
  CERTCertificate *cert = other2->GetCert();
  *result = (mCert == cert);
  if (cert) {
    CERT_DestroyCertificate(cert);
  }
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
  bool conflict;
  char* servername = nsnull;
  
  servername = CERT_GetCommonName(&cert->subject);
  if (!servername) {
    // Certs without common names are strange, but they do exist...
    // Let's try to use another string for the nickname
    servername = CERT_GetOrgUnitName(&cert->subject);
    if (!servername) {
      servername = CERT_GetOrgName(&cert->subject);
      if (!servername) {
        servername = CERT_GetLocalityName(&cert->subject);
        if (!servername) {
          servername = CERT_GetStateName(&cert->subject);
          if (!servername) {
            servername = CERT_GetCountryName(&cert->subject);
            if (!servername) {
              // We tried hard, there is nothing more we can do.
              // A cert without any names doesn't really make sense.
              return nsnull;
            }
          }
        }
      }
    }
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
    if (!conflict) {
      break;
    }
    PR_Free(nickname);
    count++;
  }
  PR_FREEIF(servername);
  return nickname;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsNSSCertList, nsIX509CertList)

nsNSSCertList::nsNSSCertList(CERTCertList *certList, bool adopt)
{
  if (certList) {
    if (adopt) {
      mCertList = certList;
    } else {
      mCertList = DupCertList(certList);
    }
  } else {
    mCertList = CERT_NewCertList();
  }
}

nsNSSCertList::~nsNSSCertList()
{
  if (mCertList) {
    CERT_DestroyCertList(mCertList);
  }
}

/* void addCert (in nsIX509Cert cert); */
NS_IMETHODIMP
nsNSSCertList::AddCert(nsIX509Cert *aCert) 
{
  /* This should be a query interface, but currently this his how the
   * rest of PSM is working */
  nsCOMPtr<nsIX509Cert2> nssCert = do_QueryInterface(aCert);
  CERTCertificate *cert;

  cert = nssCert->GetCert();
  if (cert == nsnull) {
    NS_ERROR("Somehow got nsnull for mCertificate in nsNSSCertificate.");
    return NS_ERROR_FAILURE;
  }

  if (mCertList == nsnull) {
    NS_ERROR("Somehow got nsnull for mCertList in nsNSSCertList.");
    return NS_ERROR_FAILURE;
  }
  CERT_AddCertToListTail(mCertList,cert);
  return NS_OK;
}

/* void deleteCert (in nsIX509Cert cert); */
NS_IMETHODIMP
nsNSSCertList::DeleteCert(nsIX509Cert *aCert)
{
  /* This should be a query interface, but currently this his how the
   * rest of PSM is working */
  nsCOMPtr<nsIX509Cert2> nssCert = do_QueryInterface(aCert);
  CERTCertificate *cert = nssCert->GetCert();
  CERTCertListNode *node;

  if (cert == nsnull) {
    NS_ERROR("Somehow got nsnull for mCertificate in nsNSSCertificate.");
    return NS_ERROR_FAILURE;
  }

  if (mCertList == nsnull) {
    NS_ERROR("Somehow got nsnull for mCertList in nsNSSCertList.");
    return NS_ERROR_FAILURE;
  }

  for (node = CERT_LIST_HEAD(mCertList); !CERT_LIST_END(node,mCertList);
                                             node = CERT_LIST_NEXT(node)) {
    if (node->cert == cert) {
	CERT_RemoveCertListNode(node);
        return NS_OK;
    }
  }
  return NS_OK; /* should we fail if we couldn't find it? */
}

CERTCertList *
nsNSSCertList::DupCertList(CERTCertList *aCertList)
{
  if (!aCertList)
    return nsnull;

  CERTCertList *newList = CERT_NewCertList();

  if (newList == nsnull) {
    return nsnull;
  }

  CERTCertListNode *node;
  for (node = CERT_LIST_HEAD(aCertList); !CERT_LIST_END(node, aCertList);
                                              node = CERT_LIST_NEXT(node)) {
    CERTCertificate *cert = CERT_DupCertificate(node->cert);
    CERT_AddCertToListTail(newList, cert);
  }
  return newList;
}

void *
nsNSSCertList::GetRawCertList()
{
  return mCertList;
}

/* nsISimpleEnumerator getEnumerator (); */
NS_IMETHODIMP
nsNSSCertList::GetEnumerator(nsISimpleEnumerator **_retval) 
{
  nsCOMPtr<nsISimpleEnumerator> enumerator = new nsNSSCertListEnumerator(mCertList);
  if (!enumerator) { 
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *_retval = enumerator;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsNSSCertListEnumerator, 
                              nsISimpleEnumerator)

nsNSSCertListEnumerator::nsNSSCertListEnumerator(CERTCertList *certList)
{
  mCertList = nsNSSCertList::DupCertList(certList);
}

nsNSSCertListEnumerator::~nsNSSCertListEnumerator()
{
  if (mCertList) {
    CERT_DestroyCertList(mCertList);
  }
}

/* boolean hasMoreElements (); */
NS_IMETHODIMP
nsNSSCertListEnumerator::HasMoreElements(bool *_retval)
{ 
  NS_ENSURE_TRUE(mCertList, NS_ERROR_FAILURE);

  *_retval = !CERT_LIST_EMPTY(mCertList);
  return NS_OK;
}

/* nsISupports getNext(); */
NS_IMETHODIMP
nsNSSCertListEnumerator::GetNext(nsISupports **_retval) 
{
  NS_ENSURE_TRUE(mCertList, NS_ERROR_FAILURE);

  CERTCertListNode *node = CERT_LIST_HEAD(mCertList);
  if (CERT_LIST_END(node, mCertList)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIX509Cert> nssCert = nsNSSCertificate::Create(node->cert);
  if (!nssCert) { 
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *_retval = nssCert;
  NS_ADDREF(*_retval);

  CERT_RemoveCertListNode(node);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::Write(nsIObjectOutputStream* aStream)
{
  NS_ENSURE_STATE(mCert);
  nsresult rv = aStream->Write32(mCert->derCert.len);
  if (NS_FAILED(rv)) {
    return rv;
  }
  
  return aStream->WriteByteArray(mCert->derCert.data, mCert->derCert.len);
}

NS_IMETHODIMP
nsNSSCertificate::Read(nsIObjectInputStream* aStream)
{
  NS_ENSURE_STATE(!mCert);
  
  PRUint32 len;
  nsresult rv = aStream->Read32(&len);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsXPIDLCString str;
  rv = aStream->ReadBytes(len, getter_Copies(str));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!InitFromDER(const_cast<char*>(str.get()), len)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsNSSCertificate::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  *count = 0;
  *array = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSCertificate::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSCertificate::GetContractID(char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSCertificate::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSCertificate::GetClassID(nsCID * *aClassID)
{
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP 
nsNSSCertificate::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSCertificate::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::THREADSAFE;
  return NS_OK;
}

static NS_DEFINE_CID(kNSSCertificateCID, NS_X509CERT_CID);

NS_IMETHODIMP 
nsNSSCertificate::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kNSSCertificateCID;
  return NS_OK;
}

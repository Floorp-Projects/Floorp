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
 * The Original Code is the Personal Security Manager Module
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 * Contributor(s):
 *   Honza Bambas <honzab@firemni.cz>
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

#include "nsNSSCertificateFakeTransport.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsNSSCertificate.h"
#include "nsIX509Cert.h"
#include "nsIX509Cert3.h"
#include "nsISMimeCert.h"
#include "nsNSSASN1Object.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsISupportsPrimitives.h"
#include "nsIProgrammingLanguage.h"
#include "nsIObjectOutputStream.h"
#include "nsIObjectInputStream.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

/* nsNSSCertificateFakeTransport */

NS_IMPL_THREADSAFE_ISUPPORTS5(nsNSSCertificateFakeTransport, nsIX509Cert,
                                                nsIX509Cert2,
                                                nsIX509Cert3,
                                                nsISerializable,
                                                nsIClassInfo)

nsNSSCertificateFakeTransport::nsNSSCertificateFakeTransport() :
  mCertSerialization(nsnull)
{
}

nsNSSCertificateFakeTransport::~nsNSSCertificateFakeTransport()
{
  if (mCertSerialization)
    SECITEM_FreeItem(mCertSerialization, PR_TRUE);
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetCertType(PRUint32 *aCertType)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIsSelfSigned(PRBool *aIsSelfSigned)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::MarkForPermDeletion()
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute string dbKey; */
NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetDbKey(char * *aDbKey)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute string windowTitle; */
NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetWindowTitle(char * *aWindowTitle)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetNickname(nsAString &aNickname)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetEmailAddress(nsAString &aEmailAddress)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetEmailAddresses(PRUint32 *aLength, PRUnichar*** aAddresses)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::ContainsEmailAddress(const nsAString &aEmailAddress, PRBool *result)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetCommonName(nsAString &aCommonName)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetOrganization(nsAString &aOrganization)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuerCommonName(nsAString &aCommonName)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuerOrganization(nsAString &aOrganization)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuerOrganizationUnit(nsAString &aOrganizationUnit)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIX509Cert issuer; */
NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuer(nsIX509Cert * *aIssuer)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetOrganizationalUnit(nsAString &aOrganizationalUnit)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/*
 * nsIEnumerator getChain();
 */
NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetChain(nsIArray **_rvChain)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetAllTokenNames(PRUint32 *aLength, PRUnichar*** aTokenNames)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetSubjectName(nsAString &_subjectName)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuerName(nsAString &_issuerName)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetSerialNumber(nsAString &_serialNumber)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetSha1Fingerprint(nsAString &_sha1Fingerprint)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetMd5Fingerprint(nsAString &_md5Fingerprint)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetTokenName(nsAString &aTokenName)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetRawDER(PRUint32 *aLength, PRUint8 **aArray)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::ExportAsCMS(PRUint32 chainMode,
                              PRUint32 *aLength, PRUint8 **aArray)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

CERTCertificate *
nsNSSCertificateFakeTransport::GetCert()
{
  NS_NOTREACHED("Unimplemented on content process");
  return nsnull;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetValidity(nsIX509CertValidity **aValidity)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::VerifyForUsage(PRUint32 usage, PRUint32 *verificationResult)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetUsagesArray(PRBool ignoreOcsp,
                                 PRUint32 *_verified,
                                 PRUint32 *_count,
                                 PRUnichar ***_usages)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::RequestUsagesArrayAsync(nsICertVerificationListener *aResultListener)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetUsagesString(PRBool ignoreOcsp,
                                  PRUint32   *_verified,
                                  nsAString &_usages)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIASN1Object ASN1Structure; */
NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetASN1Structure(nsIASN1Object * *aASN1Structure)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::Equals(nsIX509Cert *other, PRBool *result)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::Write(nsIObjectOutputStream* aStream)
{
  // On a non-chrome process we don't have mCert because we lack
  // nsNSSComponent.  nsNSSCertificateFakeTransport object is used only to carry the
  // certificate serialization.

  nsresult rv = aStream->Write32(mCertSerialization->len);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return aStream->WriteByteArray(mCertSerialization->data, mCertSerialization->len);
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::Read(nsIObjectInputStream* aStream)
{
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

  // On a non-chrome process we cannot instatiate mCert because we lack
  // nsNSSComponent.  nsNSSCertificateFakeTransport object is used only to carry the
  // certificate serialization.

  mCertSerialization = SECITEM_AllocItem(nsnull, nsnull, len);
  if (!mCertSerialization)
      return NS_ERROR_OUT_OF_MEMORY;
  PORT_Memcpy(mCertSerialization->data, str.Data(), len);

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetInterfaces(PRUint32 *count, nsIID * **array)
{
  *count = 0;
  *array = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetContractID(char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetClassID(nsCID * *aClassID)
{
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::THREADSAFE;
  return NS_OK;
}

static NS_DEFINE_CID(kNSSCertificateCID, NS_X509CERT_CID);

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kNSSCertificateCID;
  return NS_OK;
}

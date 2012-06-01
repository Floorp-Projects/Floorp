/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertificateFakeTransport.h"
#include "nsCOMPtr.h"
#include "nsNSSCertificate.h"
#include "nsIX509Cert.h"
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

NS_IMPL_THREADSAFE_ISUPPORTS3(nsNSSCertificateFakeTransport, nsIX509Cert,
                                                nsISerializable,
                                                nsIClassInfo)

nsNSSCertificateFakeTransport::nsNSSCertificateFakeTransport() :
  mCertSerialization(nsnull)
{
}

nsNSSCertificateFakeTransport::~nsNSSCertificateFakeTransport()
{
  if (mCertSerialization)
    SECITEM_FreeItem(mCertSerialization, true);
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
nsNSSCertificateFakeTransport::ContainsEmailAddress(const nsAString &aEmailAddress, bool *result)
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
nsNSSCertificateFakeTransport::GetUsagesArray(bool localOnly,
                                 PRUint32 *_verified,
                                 PRUint32 *_count,
                                 PRUnichar ***_usages)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetUsagesString(bool localOnly,
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
nsNSSCertificateFakeTransport::Equals(nsIX509Cert *other, bool *result)
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

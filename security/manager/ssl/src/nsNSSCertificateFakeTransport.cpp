/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertificateFakeTransport.h"

#include "nsCOMPtr.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIProgrammingLanguage.h"
#include "nsISupportsPrimitives.h"
#include "nsIX509Cert.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertificate.h"
#include "nsString.h"
#include "nsXPIDLString.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

/* nsNSSCertificateFakeTransport */

NS_IMPL_ISUPPORTS(nsNSSCertificateFakeTransport,
                  nsIX509Cert,
                  nsISerializable,
                  nsIClassInfo)

nsNSSCertificateFakeTransport::nsNSSCertificateFakeTransport() :
  mCertSerialization(nullptr)
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
nsNSSCertificateFakeTransport::GetEmailAddresses(uint32_t *aLength, char16_t*** aAddresses)
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
nsNSSCertificateFakeTransport::GetSha256Fingerprint(nsAString& aSha256Fingerprint)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetSha1Fingerprint(nsAString& aSha1Fingerprint)
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
nsNSSCertificateFakeTransport::GetRawDER(uint32_t *aLength, uint8_t **aArray)
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
nsNSSCertificateFakeTransport::GetUsagesArray(bool localOnly,
                                 uint32_t *_verified,
                                 uint32_t *_count,
                                 char16_t ***_usages)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetUsagesString(bool localOnly,
                                  uint32_t   *_verified,
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
nsNSSCertificateFakeTransport::GetSha256SubjectPublicKeyInfoDigest(nsACString_internal&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// NB: This serialization must match that of nsNSSCertificate.
NS_IMETHODIMP
nsNSSCertificateFakeTransport::Write(nsIObjectOutputStream* aStream)
{
  // On a non-chrome process we don't have mCert because we lack
  // nsNSSComponent.  nsNSSCertificateFakeTransport object is used only to carry the
  // certificate serialization.

  // This serialization has to match that of nsNSSCertificate,
  // so write a fake cached EV Status.
  uint32_t status = static_cast<uint32_t>(nsNSSCertificate::ev_status_unknown);
  nsresult rv = aStream->Write32(status);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->Write32(mCertSerialization->len);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return aStream->WriteByteArray(mCertSerialization->data, mCertSerialization->len);
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::Read(nsIObjectInputStream* aStream)
{
  // This serialization has to match that of nsNSSCertificate,
  // so read the cachedEVStatus but don't actually use it.
  uint32_t cachedEVStatus;
  nsresult rv = aStream->Read32(&cachedEVStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t len;
  rv = aStream->Read32(&len);
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

  mCertSerialization = SECITEM_AllocItem(nullptr, nullptr, len);
  if (!mCertSerialization)
      return NS_ERROR_OUT_OF_MEMORY;
  PORT_Memcpy(mCertSerialization->data, str.Data(), len);

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetInterfaces(uint32_t *count, nsIID * **array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetHelperForLanguage(uint32_t language, nsISupports **_retval)
{
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetContractID(char * *aContractID)
{
  *aContractID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nullptr;
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
nsNSSCertificateFakeTransport::GetImplementationLanguage(uint32_t *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetFlags(uint32_t *aFlags)
{
  *aFlags = nsIClassInfo::THREADSAFE;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  static NS_DEFINE_CID(kNSSCertificateCID, NS_X509CERT_CID);

  *aClassIDNoAlloc = kNSSCertificateCID;
  return NS_OK;
}

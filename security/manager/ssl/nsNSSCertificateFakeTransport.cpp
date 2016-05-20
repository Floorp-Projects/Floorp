/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertificateFakeTransport.h"

#include "nsIClassInfoImpl.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsISupportsPrimitives.h"
#include "nsNSSCertificate.h"
#include "nsString.h"
#include "nsXPIDLString.h"

NS_IMPL_ISUPPORTS(nsNSSCertificateFakeTransport,
                  nsIX509Cert,
                  nsISerializable,
                  nsIClassInfo)

nsNSSCertificateFakeTransport::nsNSSCertificateFakeTransport()
  : mCertSerialization(nullptr)
{
}

nsNSSCertificateFakeTransport::~nsNSSCertificateFakeTransport()
{
  if (mCertSerialization) {
    SECITEM_FreeItem(mCertSerialization, true);
  }
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetDbKey(nsACString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetWindowTitle(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetNickname(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetEmailAddress(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetEmailAddresses(uint32_t*, char16_t***)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::ContainsEmailAddress(const nsAString&, bool*)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetCommonName(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetOrganization(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuerCommonName(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuerOrganization(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuerOrganizationUnit(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuer(nsIX509Cert**)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetOrganizationalUnit(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetChain(nsIArray**)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetSubjectName(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIssuerName(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetSerialNumber(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetSha256Fingerprint(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetSha1Fingerprint(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetTokenName(nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetRawDER(uint32_t*, uint8_t**)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetValidity(nsIX509CertValidity**)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetUsagesArray(bool, uint32_t*, uint32_t*,
                                              char16_t***)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetUsagesString(bool, uint32_t*, nsAString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetASN1Structure(nsIASN1Object**)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::Equals(nsIX509Cert*, bool*)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetSha256SubjectPublicKeyInfoDigest(nsACString&)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// NB: This serialization must match that of nsNSSCertificate.
NS_IMETHODIMP
nsNSSCertificateFakeTransport::Write(nsIObjectOutputStream* aStream)
{
  // On a non-chrome process we don't have mCert because we lack
  // nsNSSComponent. nsNSSCertificateFakeTransport object is used only to
  // carry the certificate serialization.

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

  return aStream->WriteByteArray(mCertSerialization->data,
                                 mCertSerialization->len);
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
  // nsNSSComponent. nsNSSCertificateFakeTransport object is used only to
  // carry the certificate serialization.

  mCertSerialization = SECITEM_AllocItem(nullptr, nullptr, len);
  if (!mCertSerialization)
      return NS_ERROR_OUT_OF_MEMORY;
  PORT_Memcpy(mCertSerialization->data, str.Data(), len);

  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetInterfaces(uint32_t* count, nsIID*** array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetScriptableHelper(nsIXPCScriptable** _retval)
{
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetContractID(char** aContractID)
{
  *aContractID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetClassDescription(char** aClassDescription)
{
  *aClassDescription = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetClassID(nsCID** aClassID)
{
  *aClassID = (nsCID*) moz_xmalloc(sizeof(nsCID));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetFlags(uint32_t* aFlags)
{
  *aFlags = nsIClassInfo::THREADSAFE;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
{
  static NS_DEFINE_CID(kNSSCertificateCID, NS_X509CERT_CID);

  *aClassIDNoAlloc = kNSSCertificateCID;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetCertType(unsigned int*)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIsSelfSigned(bool*)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetIsBuiltInRoot(bool* aIsBuiltInRoot)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::RequestUsagesArrayAsync(
  nsICertVerificationListener*)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::GetAllTokenNames(unsigned int*, char16_t***)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

CERTCertificate*
nsNSSCertificateFakeTransport::GetCert()
{
  NS_NOTREACHED("Unimplemented on content process");
  return nullptr;
}

NS_IMETHODIMP
nsNSSCertificateFakeTransport::ExportAsCMS(unsigned int,
                                           unsigned int*,
                                           unsigned char**)
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

NS_IMPL_CLASSINFO(nsNSSCertListFakeTransport,
                  nullptr,
                  // inferred from nsIX509Cert
                  nsIClassInfo::THREADSAFE,
                  NS_X509CERTLIST_CID)

NS_IMPL_ISUPPORTS_CI(nsNSSCertListFakeTransport,
                     nsIX509CertList,
                     nsISerializable)

nsNSSCertListFakeTransport::nsNSSCertListFakeTransport()
{
}

nsNSSCertListFakeTransport::~nsNSSCertListFakeTransport()
{
}

NS_IMETHODIMP
nsNSSCertListFakeTransport::AddCert(nsIX509Cert* aCert)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertListFakeTransport::DeleteCert(nsIX509Cert* aCert)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

CERTCertList*
nsNSSCertListFakeTransport::GetRawCertList()
{
  NS_NOTREACHED("Unimplemented on content process");
  return nullptr;
}

NS_IMETHODIMP
nsNSSCertListFakeTransport::GetEnumerator(nsISimpleEnumerator**)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSCertListFakeTransport::Equals(nsIX509CertList*, bool*)
{
  NS_NOTREACHED("Unimplemented on content process");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// NB: This serialization must match that of nsNSSCertList.
NS_IMETHODIMP
nsNSSCertListFakeTransport::Write(nsIObjectOutputStream* aStream)
{
  uint32_t certListLen = mFakeCertList.length();
  // Write the length of the list
  nsresult rv = aStream->Write32(certListLen);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (size_t i = 0; i < certListLen; i++) {
    nsCOMPtr<nsIX509Cert> cert = mFakeCertList[i];
    nsCOMPtr<nsISerializable> serializableCert = do_QueryInterface(cert);
    rv = aStream->WriteCompoundObject(serializableCert,
                                      NS_GET_IID(nsIX509Cert), true);
    if (NS_FAILED(rv)) {
      break;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsNSSCertListFakeTransport::Read(nsIObjectInputStream* aStream)
{
  uint32_t certListLen;
  nsresult rv = aStream->Read32(&certListLen);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (uint32_t i = 0; i < certListLen; i++) {
    nsCOMPtr<nsISupports> certSupports;
    rv = aStream->ReadObject(true, getter_AddRefs(certSupports));
    if (NS_FAILED(rv)) {
      break;
    }

    nsCOMPtr<nsIX509Cert> cert = do_QueryInterface(certSupports);
    if (!mFakeCertList.append(cert)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return rv;
}

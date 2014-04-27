/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSSLStatus.h"
#include "plstr.h"
#include "nsIClassInfoImpl.h"
#include "nsIIdentityInfo.h"
#include "nsIProgrammingLanguage.h"
#include "nsIObjectOutputStream.h"
#include "nsIObjectInputStream.h"

NS_IMETHODIMP
nsSSLStatus::GetServerCert(nsIX509Cert** _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = mServerCert;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetKeyLength(uint32_t* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");
  if (!mHaveKeyLengthAndCipher)
    return NS_ERROR_NOT_AVAILABLE;

  *_result = mKeyLength;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetSecretKeyLength(uint32_t* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");
  if (!mHaveKeyLengthAndCipher)
    return NS_ERROR_NOT_AVAILABLE;

  *_result = mSecretKeyLength;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetCipherName(char** _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");
  if (!mHaveKeyLengthAndCipher)
    return NS_ERROR_NOT_AVAILABLE;

  *_result = ToNewCString(mCipherName);

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsDomainMismatch(bool* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = mHaveCertErrorBits && mIsDomainMismatch;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsNotValidAtThisTime(bool* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = mHaveCertErrorBits && mIsNotValidAtThisTime;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsUntrusted(bool* _result)
{
  NS_ASSERTION(_result, "non-NULL destination required");

  *_result = mHaveCertErrorBits && mIsUntrusted;

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsExtendedValidation(bool* aIsEV)
{
  NS_ENSURE_ARG_POINTER(aIsEV);
  *aIsEV = false;

#ifdef MOZ_NO_EV_CERTS
  return NS_OK;
#else
  nsCOMPtr<nsIX509Cert> cert = mServerCert;
  nsresult rv;
  nsCOMPtr<nsIIdentityInfo> idinfo = do_QueryInterface(cert, &rv);

  // mServerCert should never be null when this method is called because
  // nsSSLStatus objects always have mServerCert set right after they are
  // constructed and before they are returned. GetIsExtendedValidation should
  // only be called in the chrome process (in e10s), and mServerCert will always
  // implement nsIIdentityInfo in the chrome process.
  if (!idinfo) {
    NS_ERROR("nsSSLStatus has null mServerCert or was called in the content "
             "process");
    return NS_ERROR_UNEXPECTED;
  }

  // Never allow bad certs for EV, regardless of overrides.
  if (mHaveCertErrorBits) {
    return NS_OK;
  }

  return idinfo->GetIsExtendedValidation(aIsEV);
#endif
}

NS_IMETHODIMP
nsSSLStatus::Read(nsIObjectInputStream* stream)
{
  nsCOMPtr<nsISupports> cert;
  nsresult rv = stream->ReadObject(true, getter_AddRefs(cert));
  NS_ENSURE_SUCCESS(rv, rv);

  mServerCert = do_QueryInterface(cert);
  if (!mServerCert)
    return NS_NOINTERFACE;

  rv = stream->Read32(&mKeyLength);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->Read32(&mSecretKeyLength);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->ReadCString(mCipherName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->ReadBoolean(&mIsDomainMismatch);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->ReadBoolean(&mIsNotValidAtThisTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->ReadBoolean(&mIsUntrusted);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->ReadBoolean(&mHaveKeyLengthAndCipher);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->ReadBoolean(&mHaveCertErrorBits);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::Write(nsIObjectOutputStream* stream)
{
  nsresult rv = stream->WriteCompoundObject(mServerCert,
                                            NS_GET_IID(nsIX509Cert),
                                            true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->Write32(mKeyLength);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->Write32(mSecretKeyLength);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->WriteStringZ(mCipherName.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->WriteBoolean(mIsDomainMismatch);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->WriteBoolean(mIsNotValidAtThisTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->WriteBoolean(mIsUntrusted);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->WriteBoolean(mHaveKeyLengthAndCipher);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stream->WriteBoolean(mHaveCertErrorBits);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetInterfaces(uint32_t *count, nsIID * **array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetHelperForLanguage(uint32_t language, nsISupports **_retval)
{
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetContractID(char * *aContractID)
{
  *aContractID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetClassID(nsCID * *aClassID)
{
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
nsSSLStatus::GetImplementationLanguage(uint32_t *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetFlags(uint32_t *aFlags)
{
  *aFlags = 0;
  return NS_OK;
}

static NS_DEFINE_CID(kSSLStatusCID, NS_SSLSTATUS_CID);

NS_IMETHODIMP
nsSSLStatus::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kSSLStatusCID;
  return NS_OK;
}

nsSSLStatus::nsSSLStatus()
: mKeyLength(0), mSecretKeyLength(0)
, mIsDomainMismatch(false)
, mIsNotValidAtThisTime(false)
, mIsUntrusted(false)
, mHaveKeyLengthAndCipher(false)
, mHaveCertErrorBits(false)
{
  mCipherName = "";
}

NS_IMPL_ISUPPORTS(nsSSLStatus, nsISSLStatus, nsISerializable, nsIClassInfo)

nsSSLStatus::~nsSSLStatus()
{
}

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
#include "ssl.h"

NS_IMETHODIMP
nsSSLStatus::GetServerCert(nsIX509Cert** aServerCert)
{
  NS_ENSURE_ARG_POINTER(aServerCert);

  nsCOMPtr<nsIX509Cert> cert = mServerCert;
  cert.forget(aServerCert);
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetKeyLength(uint32_t* aKeyLength)
{
  NS_ENSURE_ARG_POINTER(aKeyLength);
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(mCipherSuite, &cipherInfo,
                             sizeof(cipherInfo)) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  *aKeyLength = cipherInfo.symKeyBits;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetSecretKeyLength(uint32_t* aSecretKeyLength)
{
  NS_ENSURE_ARG_POINTER(aSecretKeyLength);
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(mCipherSuite, &cipherInfo,
                             sizeof(cipherInfo)) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  *aSecretKeyLength = cipherInfo.effectiveKeyBits;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetCipherName(nsACString& aCipherName)
{
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(mCipherSuite, &cipherInfo,
                             sizeof(cipherInfo)) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  aCipherName.Assign(cipherInfo.cipherSuiteName);
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetProtocolVersion(uint16_t* aProtocolVersion)
{
  NS_ENSURE_ARG_POINTER(aProtocolVersion);
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aProtocolVersion = mProtocolVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsDomainMismatch(bool* aIsDomainMismatch)
{
  NS_ENSURE_ARG_POINTER(aIsDomainMismatch);

  *aIsDomainMismatch = mHaveCertErrorBits && mIsDomainMismatch;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsNotValidAtThisTime(bool* aIsNotValidAtThisTime)
{
  NS_ENSURE_ARG_POINTER(aIsNotValidAtThisTime);

  *aIsNotValidAtThisTime = mHaveCertErrorBits && mIsNotValidAtThisTime;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetIsUntrusted(bool* aIsUntrusted)
{
  NS_ENSURE_ARG_POINTER(aIsUntrusted);

  *aIsUntrusted = mHaveCertErrorBits && mIsUntrusted;
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
  // mServerCert should never be null when this method is called because
  // nsSSLStatus objects always have mServerCert set right after they are
  // constructed and before they are returned. GetIsExtendedValidation should
  // only be called in the chrome process (in e10s), and mServerCert will always
  // implement nsIIdentityInfo in the chrome process.
  nsCOMPtr<nsIIdentityInfo> idinfo = do_QueryInterface(cert);
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
nsSSLStatus::Read(nsIObjectInputStream* aStream)
{
  nsCOMPtr<nsISupports> cert;
  nsresult rv = aStream->ReadObject(true, getter_AddRefs(cert));
  NS_ENSURE_SUCCESS(rv, rv);

  mServerCert = do_QueryInterface(cert);
  if (!mServerCert) {
    return NS_NOINTERFACE;
  }

  rv = aStream->Read16(&mCipherSuite);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->Read16(&mProtocolVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->ReadBoolean(&mIsDomainMismatch);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mIsNotValidAtThisTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mIsUntrusted);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->ReadBoolean(&mHaveCipherSuiteAndProtocol);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mHaveCertErrorBits);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv = aStream->WriteCompoundObject(mServerCert,
                                             NS_GET_IID(nsIX509Cert),
                                             true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->Write16(mCipherSuite);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->Write16(mProtocolVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteBoolean(mIsDomainMismatch);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mIsNotValidAtThisTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mIsUntrusted);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteBoolean(mHaveCipherSuiteAndProtocol);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mHaveCertErrorBits);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetInterfaces(uint32_t* aCount, nsIID*** aArray)
{
  *aCount = 0;
  *aArray = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetHelperForLanguage(uint32_t aLanguage, nsISupports** aHelper)
{
  *aHelper = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetContractID(char** aContractID)
{
  *aContractID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetClassDescription(char** aClassDescription)
{
  *aClassDescription = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetClassID(nsCID** aClassID)
{
  *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
  if (!*aClassID) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
nsSSLStatus::GetImplementationLanguage(uint32_t* aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::GetFlags(uint32_t* aFlags)
{
  *aFlags = 0;
  return NS_OK;
}

static NS_DEFINE_CID(kSSLStatusCID, NS_SSLSTATUS_CID);

NS_IMETHODIMP
nsSSLStatus::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kSSLStatusCID;
  return NS_OK;
}

nsSSLStatus::nsSSLStatus()
: mCipherSuite(0)
, mProtocolVersion(0)
, mIsDomainMismatch(false)
, mIsNotValidAtThisTime(false)
, mIsUntrusted(false)
, mHaveCipherSuiteAndProtocol(false)
, mHaveCertErrorBits(false)
{
}

NS_IMPL_ISUPPORTS(nsSSLStatus, nsISSLStatus, nsISerializable, nsIClassInfo)

nsSSLStatus::~nsSSLStatus()
{
}

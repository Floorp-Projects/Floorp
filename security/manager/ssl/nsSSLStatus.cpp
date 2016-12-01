/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTVerifyResult.h"
#include "mozilla/Casting.h"
#include "nsSSLStatus.h"
#include "nsIClassInfoImpl.h"
#include "nsIObjectOutputStream.h"
#include "nsIObjectInputStream.h"
#include "nsNSSCertificate.h"
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
nsSSLStatus::GetCertificateTransparencyStatus(
  uint16_t* aCertificateTransparencyStatus)
{
  NS_ENSURE_ARG_POINTER(aCertificateTransparencyStatus);

  *aCertificateTransparencyStatus = mCertificateTransparencyStatus;
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

  // Never allow bad certs for EV, regardless of overrides.
  if (mHaveCertErrorBits) {
    return NS_OK;
  }

  if (mHasIsEVStatus) {
    *aIsEV = mIsEV;
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
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

  // The code below is a workaround to allow serializing new fields
  // while preserving binary compatibility with older streams. For more details
  // on the binary compatibility requirement, refer to bug 1248628.
  // Here, we take advantage of the fact that mProtocolVersion was originally
  // stored as a 16 bits integer, but the highest 8 bits were never used.
  // These bits are now used for stream versioning.
  uint16_t protocolVersionAndStreamFormatVersion;
  rv = aStream->Read16(&protocolVersionAndStreamFormatVersion);
  NS_ENSURE_SUCCESS(rv, rv);
  mProtocolVersion = protocolVersionAndStreamFormatVersion & 0xFF;
  const uint8_t streamFormatVersion =
    (protocolVersionAndStreamFormatVersion >> 8) & 0xFF;

  rv = aStream->ReadBoolean(&mIsDomainMismatch);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mIsNotValidAtThisTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mIsUntrusted);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mIsEV);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->ReadBoolean(&mHasIsEVStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mHaveCipherSuiteAndProtocol);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mHaveCertErrorBits);
  NS_ENSURE_SUCCESS(rv, rv);

  // Added in version 1 (see bug 1305289).
  if (streamFormatVersion >= 1) {
    rv = aStream->Read16(&mCertificateTransparencyStatus);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSSLStatus::Write(nsIObjectOutputStream* aStream)
{
  // The current version of the binary stream format.
  const uint8_t STREAM_FORMAT_VERSION = 1;

  nsresult rv = aStream->WriteCompoundObject(mServerCert,
                                             NS_GET_IID(nsIX509Cert),
                                             true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->Write16(mCipherSuite);
  NS_ENSURE_SUCCESS(rv, rv);

  uint16_t protocolVersionAndStreamFormatVersion =
    mozilla::AssertedCast<uint8_t>(mProtocolVersion) |
    (STREAM_FORMAT_VERSION << 8);
  rv = aStream->Write16(protocolVersionAndStreamFormatVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteBoolean(mIsDomainMismatch);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mIsNotValidAtThisTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mIsUntrusted);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mIsEV);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteBoolean(mHasIsEVStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mHaveCipherSuiteAndProtocol);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mHaveCertErrorBits);
  NS_ENSURE_SUCCESS(rv, rv);

  // Added in version 1.
  rv = aStream->Write16(mCertificateTransparencyStatus);
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
nsSSLStatus::GetScriptableHelper(nsIXPCScriptable** aHelper)
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
  *aClassID = (nsCID*) moz_xmalloc(sizeof(nsCID));
  if (!*aClassID) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return GetClassIDNoAlloc(*aClassID);
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
, mCertificateTransparencyStatus(nsISSLStatus::
    CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE)
, mIsDomainMismatch(false)
, mIsNotValidAtThisTime(false)
, mIsUntrusted(false)
, mIsEV(false)
, mHasIsEVStatus(false)
, mHaveCipherSuiteAndProtocol(false)
, mHaveCertErrorBits(false)
{
}

NS_IMPL_ISUPPORTS(nsSSLStatus, nsISSLStatus, nsISerializable, nsIClassInfo)

nsSSLStatus::~nsSSLStatus()
{
}

void
nsSSLStatus::SetServerCert(nsNSSCertificate* aServerCert, EVStatus aEVStatus)
{
  MOZ_ASSERT(aServerCert);

  mServerCert = aServerCert;
  mIsEV = (aEVStatus == EVStatus::EV);
  mHasIsEVStatus = true;
}

void
nsSSLStatus::SetCertificateTransparencyInfo(
  const mozilla::psm::CertificateTransparencyInfo& info)
{
  using mozilla::ct::VerifiedSCT;

  if (!info.enabled) {
    // CT disabled.
    mCertificateTransparencyStatus =
      nsISSLStatus::CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE;
    return;
  }

  if (!info.processedSCTs) {
    // No SCTs processed on the connection.
    mCertificateTransparencyStatus =
      nsISSLStatus::CERTIFICATE_TRANSPARENCY_NONE;
    return;
  }

  bool hasValidSCTs = false;
  bool hasUnknownLogSCTs = false;
  bool hasInvalidSCTs = false;
  for (const VerifiedSCT& verifiedSct : info.verifyResult.verifiedScts) {
    switch (verifiedSct.status) {
      case VerifiedSCT::Status::Valid:
        hasValidSCTs = true;
        break;
      case VerifiedSCT::Status::UnknownLog:
      case VerifiedSCT::Status::ValidFromDisqualifiedLog:
        hasUnknownLogSCTs = true;
        break;
      case VerifiedSCT::Status::InvalidSignature:
      case VerifiedSCT::Status::InvalidTimestamp:
        hasInvalidSCTs = true;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected VerifiedSCT::Status type");
    }
  }

  if (hasValidSCTs) {
    mCertificateTransparencyStatus =
      nsISSLStatus::CERTIFICATE_TRANSPARENCY_OK;
  } else if (hasUnknownLogSCTs) {
    mCertificateTransparencyStatus =
      nsISSLStatus::CERTIFICATE_TRANSPARENCY_UNKNOWN_LOG;
  } else if (hasInvalidSCTs) {
    mCertificateTransparencyStatus =
      nsISSLStatus::CERTIFICATE_TRANSPARENCY_INVALID;
  }
}

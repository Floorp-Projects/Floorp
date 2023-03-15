/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransportSecurityInfo.h"

#include "ipc/IPCMessageUtils.h"
#include "mozilla/Base64.h"
#include "mozpkix/pkixtypes.h"
#include "nsBase64Encoder.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIWebProgressListener.h"
#include "nsNSSCertHelper.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "nsXULAppAPI.h"
#include "nsIX509Cert.h"
#include "secerr.h"
#include "ssl.h"

#include "mozilla/ipc/IPDLParamTraits.h"

// nsITransportSecurityInfo should not be created via do_CreateInstance. This
// stub prevents that.
template <>
already_AddRefed<nsISupports>
mozCreateComponent<mozilla::psm::TransportSecurityInfo>() {
  return nullptr;
}

namespace mozilla {
namespace psm {

TransportSecurityInfo::TransportSecurityInfo(
    uint32_t aSecurityState, PRErrorCode aErrorCode,
    nsTArray<RefPtr<nsIX509Cert>>&& aFailedCertChain,
    nsCOMPtr<nsIX509Cert>& aServerCert,
    nsTArray<RefPtr<nsIX509Cert>>&& aSucceededCertChain,
    Maybe<uint16_t> aCipherSuite, Maybe<nsCString> aKeaGroupName,
    Maybe<nsCString> aSignatureSchemeName, Maybe<uint16_t> aProtocolVersion,
    uint16_t aCertificateTransparencyStatus, Maybe<bool> aIsAcceptedEch,
    Maybe<bool> aIsDelegatedCredential,
    Maybe<OverridableErrorCategory> aOverridableErrorCategory,
    bool aMadeOCSPRequests, bool aUsedPrivateDNS, Maybe<bool> aIsEV,
    bool aNPNCompleted, const nsCString& aNegotiatedNPN, bool aResumed,
    bool aIsBuiltCertChainRootBuiltInRoot, const nsCString& aPeerId)
    : mSecurityState(aSecurityState),
      mErrorCode(aErrorCode),
      mFailedCertChain(std::move(aFailedCertChain)),
      mServerCert(aServerCert),
      mSucceededCertChain(std::move(aSucceededCertChain)),
      mCipherSuite(aCipherSuite),
      mKeaGroupName(aKeaGroupName),
      mSignatureSchemeName(aSignatureSchemeName),
      mProtocolVersion(aProtocolVersion),
      mCertificateTransparencyStatus(aCertificateTransparencyStatus),
      mIsAcceptedEch(aIsAcceptedEch),
      mIsDelegatedCredential(aIsDelegatedCredential),
      mOverridableErrorCategory(aOverridableErrorCategory),
      mMadeOCSPRequests(aMadeOCSPRequests),
      mUsedPrivateDNS(aUsedPrivateDNS),
      mIsEV(aIsEV),
      mNPNCompleted(aNPNCompleted),
      mNegotiatedNPN(aNegotiatedNPN),
      mResumed(aResumed),
      mIsBuiltCertChainRootBuiltInRoot(aIsBuiltCertChainRootBuiltInRoot),
      mPeerId(aPeerId) {}

NS_IMPL_ISUPPORTS(TransportSecurityInfo, nsITransportSecurityInfo)

NS_IMETHODIMP
TransportSecurityInfo::GetSecurityState(uint32_t* state) {
  *state = mSecurityState;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetErrorCode(int32_t* state) {
  *state = mErrorCode;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetErrorCodeString(nsAString& aErrorString) {
  const char* codeName = PR_ErrorToName(mErrorCode);
  aErrorString.Truncate();
  if (codeName) {
    aErrorString = NS_ConvertASCIItoUTF16(codeName);
  }

  return NS_OK;
}

// 16786594-0296-4471-8096-8f84497ca428
#define TRANSPORTSECURITYINFO_CID                    \
  {                                                  \
    0x16786594, 0x0296, 0x4471, {                    \
      0x80, 0x96, 0x8f, 0x84, 0x49, 0x7c, 0xa4, 0x28 \
    }                                                \
  }
static NS_DEFINE_CID(kTransportSecurityInfoCID, TRANSPORTSECURITYINFO_CID);

// This is a new magic value. However, it re-uses the first 4 bytes
// of the previous value. This is so when older versions attempt to
// read a newer serialized TransportSecurityInfo, they will actually
// fail and return NS_ERROR_FAILURE instead of silently failing.
#define TRANSPORTSECURITYINFOMAGIC                   \
  {                                                  \
    0xa9863a23, 0x1faa, 0x4169, {                    \
      0xb0, 0xd2, 0x81, 0x29, 0xec, 0x7c, 0xb1, 0xde \
    }                                                \
  }
static NS_DEFINE_CID(kTransportSecurityInfoMagic, TRANSPORTSECURITYINFOMAGIC);

NS_IMETHODIMP
TransportSecurityInfo::ToString(nsACString& aResult) {
  RefPtr<nsBase64Encoder> stream(new nsBase64Encoder());
  nsCOMPtr<nsIObjectOutputStream> objStream(NS_NewObjectOutputStream(stream));
  nsresult rv = objStream->WriteID(kTransportSecurityInfoCID);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = objStream->WriteID(NS_ISUPPORTS_IID);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->WriteID(kTransportSecurityInfoMagic);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->Write32(mSecurityState);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsBrokenSecurity was removed in bug 748809
  rv = objStream->Write32(0);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsNoSecurity was removed in bug 748809
  rv = objStream->Write32(0);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = objStream->Write32(static_cast<uint32_t>(mErrorCode));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Re-purpose mErrorMessageCached to represent serialization version
  // If string doesn't match exact version it will be treated as older
  // serialization.
  rv = objStream->WriteWStringZ(NS_ConvertUTF8toUTF16("9").get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  // moved from nsISSLStatus
  rv = NS_WriteOptionalCompoundObject(objStream, mServerCert,
                                      NS_GET_IID(nsIX509Cert), true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->Write16(mCipherSuite.isSome() ? *mCipherSuite : 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->Write16(mProtocolVersion.isSome() ? *mProtocolVersion : 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->Write32(mOverridableErrorCategory.isSome()
                              ? *mOverridableErrorCategory
                              : OverridableErrorCategory::ERROR_UNSET);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = objStream->WriteBoolean(mIsEV.isSome() ? *mIsEV : false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->WriteBoolean(mIsEV.isSome());  // previously mHasIsEV
  NS_ENSURE_SUCCESS(rv, rv);
  rv = objStream->WriteBoolean(
      mCipherSuite.isSome());  // previously mHaveCipherSuiteAndProtocol
  NS_ENSURE_SUCCESS(rv, rv);
  rv = objStream->WriteBoolean(
      mOverridableErrorCategory.isSome());  // previously mHaveCertErrorBits
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->Write16(mCertificateTransparencyStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->WriteStringZ(mKeaGroupName.isSome() ? (*mKeaGroupName).get()
                                                      : "");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->WriteStringZ(
      mSignatureSchemeName.isSome() ? (*mSignatureSchemeName).get() : "");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->Write16(mSucceededCertChain.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  for (const auto& cert : mSucceededCertChain) {
    rv = objStream->WriteCompoundObject(cert, NS_GET_IID(nsIX509Cert), true);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // END moved from nsISSLStatus
  rv = objStream->Write16(mFailedCertChain.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  for (const auto& cert : mFailedCertChain) {
    rv = objStream->WriteCompoundObject(cert, NS_GET_IID(nsIX509Cert), true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = objStream->WriteBoolean(
      mIsDelegatedCredential.isSome() ? *mIsDelegatedCredential : false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->WriteBoolean(mNPNCompleted);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->WriteStringZ(mNegotiatedNPN.get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->WriteBoolean(mResumed);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->WriteBoolean(mIsBuiltCertChainRootBuiltInRoot);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->WriteBoolean(mIsAcceptedEch.isSome() ? *mIsAcceptedEch
                                                       : false);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->WriteStringZ(mPeerId.get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->WriteBoolean(mMadeOCSPRequests);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = objStream->WriteBoolean(mUsedPrivateDNS);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = stream->Finish(aResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

nsresult TransportSecurityInfo::ReadOldOverridableErrorBits(
    nsIObjectInputStream* aStream,
    OverridableErrorCategory& aOverridableErrorCategory) {
  bool isDomainMismatch;
  nsresult rv = aStream->ReadBoolean(&isDomainMismatch);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isNotValidAtThisTime;
  rv = aStream->ReadBoolean(&isNotValidAtThisTime);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isUntrusted;
  rv = aStream->ReadBoolean(&isUntrusted);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isUntrusted) {
    aOverridableErrorCategory =
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TRUST;
  } else if (isDomainMismatch) {
    aOverridableErrorCategory =
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_DOMAIN;
  } else if (isNotValidAtThisTime) {
    aOverridableErrorCategory =
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TIME;
  } else {
    aOverridableErrorCategory =
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_UNSET;
  }

  return NS_OK;
}

// This is for backward compatibility to be able to read nsISSLStatus
// serialized object.
nsresult TransportSecurityInfo::ReadSSLStatus(
    nsIObjectInputStream* aStream, nsCOMPtr<nsIX509Cert>& aServerCert,
    Maybe<uint16_t>& aCipherSuite, Maybe<uint16_t>& aProtocolVersion,
    Maybe<OverridableErrorCategory>& aOverridableErrorCategory,
    Maybe<bool>& aIsEV, uint16_t& aCertificateTransparencyStatus,
    Maybe<nsCString>& aKeaGroupName, Maybe<nsCString>& aSignatureSchemeName,
    nsTArray<RefPtr<nsIX509Cert>>& aSucceededCertChain) {
  bool nsISSLStatusPresent;
  nsresult rv = aStream->ReadBoolean(&nsISSLStatusPresent);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!nsISSLStatusPresent) {
    return NS_OK;
  }
  // nsISSLStatus present.  Prepare to read elements.
  // Throw away cid, validate iid
  nsCID cid;
  nsIID iid;
  rv = aStream->ReadID(&cid);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadID(&iid);
  NS_ENSURE_SUCCESS(rv, rv);

  static const nsIID nsSSLStatusIID = {
      0xfa9ba95b,
      0xca3b,
      0x498a,
      {0xb8, 0x89, 0x7c, 0x79, 0xcf, 0x28, 0xfe, 0xe8}};
  if (!iid.Equals(nsSSLStatusIID)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsISupports> cert;
  rv = aStream->ReadObject(true, getter_AddRefs(cert));
  NS_ENSURE_SUCCESS(rv, rv);

  if (cert) {
    aServerCert = do_QueryInterface(cert);
    if (!aServerCert) {
      return NS_NOINTERFACE;
    }
  }

  uint16_t cipherSuite;
  rv = aStream->Read16(&cipherSuite);
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
  const uint8_t streamFormatVersion =
      (protocolVersionAndStreamFormatVersion >> 8) & 0xFF;

  OverridableErrorCategory overridableErrorCategory;
  rv = ReadOldOverridableErrorBits(aStream, overridableErrorCategory);
  NS_ENSURE_SUCCESS(rv, rv);
  bool isEV;
  rv = aStream->ReadBoolean(&isEV);
  NS_ENSURE_SUCCESS(rv, rv);
  bool hasIsEVStatus;
  rv = aStream->ReadBoolean(&hasIsEVStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasIsEVStatus) {
    aIsEV.emplace(isEV);
  }
  bool haveCipherSuiteAndProtocol;
  rv = aStream->ReadBoolean(&haveCipherSuiteAndProtocol);
  if (haveCipherSuiteAndProtocol) {
    aCipherSuite.emplace(cipherSuite);
    aProtocolVersion.emplace(protocolVersionAndStreamFormatVersion & 0xFF);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  bool haveCertErrorBits;
  rv = aStream->ReadBoolean(&haveCertErrorBits);
  NS_ENSURE_SUCCESS(rv, rv);
  if (haveCertErrorBits) {
    aOverridableErrorCategory.emplace(overridableErrorCategory);
  }

  // Added in version 1 (see bug 1305289).
  if (streamFormatVersion >= 1) {
    rv = aStream->Read16(&aCertificateTransparencyStatus);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Added in version 2 (see bug 1304923).
  if (streamFormatVersion >= 2) {
    nsCString keaGroupName;
    rv = aStream->ReadCString(keaGroupName);
    NS_ENSURE_SUCCESS(rv, rv);
    if (haveCipherSuiteAndProtocol) {
      aKeaGroupName.emplace(keaGroupName);
    }

    nsCString signatureSchemeName;
    rv = aStream->ReadCString(signatureSchemeName);
    NS_ENSURE_SUCCESS(rv, rv);
    if (haveCipherSuiteAndProtocol) {
      aSignatureSchemeName.emplace(signatureSchemeName);
    }
  }

  // Added in version 3 (see bug 1406856).
  if (streamFormatVersion >= 3) {
    rv = ReadCertList(aStream, aSucceededCertChain);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Read only to consume bytes from the stream.
    nsTArray<RefPtr<nsIX509Cert>> failedCertChain;
    rv = ReadCertList(aStream, failedCertChain);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return rv;
}

// This is for backward compatability to be able to read nsIX509CertList
// serialized object.
nsresult TransportSecurityInfo::ReadCertList(
    nsIObjectInputStream* aStream, nsTArray<RefPtr<nsIX509Cert>>& aCertList) {
  bool nsIX509CertListPresent;

  nsresult rv = aStream->ReadBoolean(&nsIX509CertListPresent);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!nsIX509CertListPresent) {
    return NS_OK;
  }
  // nsIX509CertList present.  Prepare to read elements.
  // Throw away cid, validate iid
  nsCID cid;
  nsIID iid;
  rv = aStream->ReadID(&cid);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadID(&iid);
  NS_ENSURE_SUCCESS(rv, rv);

  static const nsIID nsIX509CertListIID = {
      0xae74cda5,
      0xcd2f,
      0x473f,
      {0x96, 0xf5, 0xf0, 0xb7, 0xff, 0xf6, 0x2c, 0x68}};

  if (!iid.Equals(nsIX509CertListIID)) {
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t certListSize;
  rv = aStream->Read32(&certListSize);
  NS_ENSURE_SUCCESS(rv, rv);

  return ReadCertificatesFromStream(aStream, certListSize, aCertList);
}

nsresult TransportSecurityInfo::ReadCertificatesFromStream(
    nsIObjectInputStream* aStream, uint32_t aSize,
    nsTArray<RefPtr<nsIX509Cert>>& aCertList) {
  nsresult rv;
  for (uint32_t i = 0; i < aSize; ++i) {
    nsCOMPtr<nsISupports> support;
    rv = aStream->ReadObject(true, getter_AddRefs(support));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIX509Cert> cert = do_QueryInterface(support);
    if (!cert) {
      return NS_ERROR_UNEXPECTED;
    }
    RefPtr<nsIX509Cert> castedCert(cert.get());
    aCertList.AppendElement(castedCert);
  }
  return NS_OK;
}

static nsITransportSecurityInfo::OverridableErrorCategory
IntToOverridableErrorCategory(uint32_t intVal) {
  switch (intVal) {
    case static_cast<uint32_t>(
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TRUST):
      return nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TRUST;
    case static_cast<uint32_t>(
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_DOMAIN):
      return nsITransportSecurityInfo::OverridableErrorCategory::ERROR_DOMAIN;
    case static_cast<uint32_t>(
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TIME):
      return nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TIME;
    default:
      break;
  }
  return nsITransportSecurityInfo::OverridableErrorCategory::ERROR_UNSET;
}

nsresult TransportSecurityInfo::Read(const nsCString& aSerializedSecurityInfo,
                                     nsITransportSecurityInfo** aResult) {
  *aResult = nullptr;

  nsCString decodedSecurityInfo;
  nsresult rv = Base64Decode(aSerializedSecurityInfo, decodedSecurityInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIInputStream> inputStream;
  rv = NS_NewCStringInputStream(getter_AddRefs(inputStream),
                                std::move(decodedSecurityInfo));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIObjectInputStream> objStream(
      NS_NewObjectInputStream(inputStream));
  if (!objStream) {
    return rv;
  }

  nsCID cid;
  rv = objStream->ReadID(&cid);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!cid.Equals(kTransportSecurityInfoCID)) {
    return NS_ERROR_UNEXPECTED;
  }
  nsIID iid;
  rv = objStream->ReadID(&iid);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!iid.Equals(NS_ISUPPORTS_IID)) {
    return rv;
  }

  nsID id;
  rv = objStream->ReadID(&id);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!id.Equals(kTransportSecurityInfoMagic)) {
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t aSecurityState = 0;
  PRErrorCode aErrorCode = 0;
  nsTArray<RefPtr<nsIX509Cert>> aFailedCertChain;
  nsCOMPtr<nsIX509Cert> aServerCert;
  nsTArray<RefPtr<nsIX509Cert>> aSucceededCertChain;
  Maybe<uint16_t> aCipherSuite;
  Maybe<nsCString> aKeaGroupName;
  Maybe<nsCString> aSignatureSchemeName;
  Maybe<uint16_t> aProtocolVersion;
  uint16_t aCertificateTransparencyStatus;
  Maybe<bool> aIsAcceptedEch;
  Maybe<bool> aIsDelegatedCredential;
  Maybe<OverridableErrorCategory> aOverridableErrorCategory;
  bool aMadeOCSPRequests = false;
  bool aUsedPrivateDNS = false;
  Maybe<bool> aIsEV;
  bool aNPNCompleted = false;
  nsCString aNegotiatedNPN;
  bool aResumed = false;
  bool aIsBuiltCertChainRootBuiltInRoot = false;
  nsCString aPeerId;
  rv = objStream->Read32(&aSecurityState);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsBrokenSecurity was removed in bug 748809
  uint32_t unusedSubRequestsBrokenSecurity;
  rv = objStream->Read32(&unusedSubRequestsBrokenSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsNoSecurity was removed in bug 748809
  uint32_t unusedSubRequestsNoSecurity;
  rv = objStream->Read32(&unusedSubRequestsNoSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint32_t errorCode;
  rv = objStream->Read32(&errorCode);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // PRErrorCode will be a negative value
  aErrorCode = static_cast<PRErrorCode>(errorCode);

  // Re-purpose mErrorMessageCached to represent serialization version
  // If string doesn't match exact version it will be treated as older
  // serialization.
  nsAutoString serVersion;
  rv = objStream->ReadString(serVersion);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t serVersionParsedToInt = 0;

  if (!serVersion.IsEmpty()) {
    char first = serVersion.First();
    // Check whether the first character of serVersion is a number
    // since ToInteger() skipps some non integer values.
    if (first >= '0' && first <= '9') {
      nsresult error = NS_OK;
      serVersionParsedToInt = serVersion.ToInteger(&error);
      if (NS_FAILED(error)) {
        return error;
      }
    }
  }

  // moved from nsISSLStatus
  if (serVersionParsedToInt < 1) {
    // nsISSLStatus may be present
    rv = ReadSSLStatus(objStream, aServerCert, aCipherSuite, aProtocolVersion,
                       aOverridableErrorCategory, aIsEV,
                       aCertificateTransparencyStatus, aKeaGroupName,
                       aSignatureSchemeName, aSucceededCertChain);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsCOMPtr<nsISupports> cert;
    rv = NS_ReadOptionalObject(objStream, true, getter_AddRefs(cert));
    NS_ENSURE_SUCCESS(rv, rv);

    if (cert) {
      aServerCert = do_QueryInterface(cert);
      if (!aServerCert) {
        return NS_NOINTERFACE;
      }
    }

    uint16_t cipherSuite;
    rv = objStream->Read16(&cipherSuite);
    NS_ENSURE_SUCCESS(rv, rv);

    uint16_t protocolVersion;
    rv = objStream->Read16(&protocolVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    OverridableErrorCategory overridableErrorCategory;
    if (serVersionParsedToInt < 8) {
      rv = ReadOldOverridableErrorBits(objStream, overridableErrorCategory);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      uint32_t overridableErrorCategoryInt;
      rv = objStream->Read32(&overridableErrorCategoryInt);
      NS_ENSURE_SUCCESS(rv, rv);
      overridableErrorCategory =
          IntToOverridableErrorCategory(overridableErrorCategoryInt);
    }
    bool isEV;
    rv = objStream->ReadBoolean(&isEV);
    NS_ENSURE_SUCCESS(rv, rv);
    bool hasIsEVStatus;
    rv = objStream->ReadBoolean(&hasIsEVStatus);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasIsEVStatus) {
      aIsEV.emplace(isEV);
    }
    bool haveCipherSuiteAndProtocol;
    rv = objStream->ReadBoolean(&haveCipherSuiteAndProtocol);
    NS_ENSURE_SUCCESS(rv, rv);
    if (haveCipherSuiteAndProtocol) {
      aCipherSuite.emplace(cipherSuite);
      aProtocolVersion.emplace(protocolVersion);
    }
    bool haveCertErrorBits;
    rv = objStream->ReadBoolean(&haveCertErrorBits);
    NS_ENSURE_SUCCESS(rv, rv);
    if (haveCertErrorBits) {
      aOverridableErrorCategory.emplace(overridableErrorCategory);
    }

    rv = objStream->Read16(&aCertificateTransparencyStatus);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString keaGroupName;
    rv = objStream->ReadCString(keaGroupName);
    NS_ENSURE_SUCCESS(rv, rv);
    if (haveCipherSuiteAndProtocol) {
      aKeaGroupName.emplace(keaGroupName);
    }

    nsCString signatureSchemeName;
    rv = objStream->ReadCString(signatureSchemeName);
    NS_ENSURE_SUCCESS(rv, rv);
    if (haveCipherSuiteAndProtocol) {
      aSignatureSchemeName.emplace(signatureSchemeName);
    }

    if (serVersionParsedToInt < 3) {
      // The old data structure of certList(nsIX509CertList) presents
      rv = ReadCertList(objStream, aSucceededCertChain);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      uint16_t certCount;
      rv = objStream->Read16(&certCount);
      NS_ENSURE_SUCCESS(rv, rv);

      rv =
          ReadCertificatesFromStream(objStream, certCount, aSucceededCertChain);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  // END moved from nsISSLStatus
  if (serVersionParsedToInt < 3) {
    // The old data structure of certList(nsIX509CertList) presents
    rv = ReadCertList(objStream, aFailedCertChain);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    uint16_t certCount;
    rv = objStream->Read16(&certCount);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ReadCertificatesFromStream(objStream, certCount, aFailedCertChain);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // mIsDelegatedCredential added in bug 1562773
  if (serVersionParsedToInt >= 2) {
    bool isDelegatedCredential;
    rv = objStream->ReadBoolean(&isDelegatedCredential);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // If aCipherSuite is Some, the serialized TransportSecurityinfo had its
    // cipher suite and protocol information, which means it has this
    // information.
    if (aCipherSuite.isSome()) {
      aIsDelegatedCredential.emplace(isDelegatedCredential);
    }
  }

  // mNPNCompleted, mNegotiatedNPN, mResumed added in bug 1584104
  if (serVersionParsedToInt >= 4) {
    rv = objStream->ReadBoolean(&aNPNCompleted);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = objStream->ReadCString(aNegotiatedNPN);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = objStream->ReadBoolean(&aResumed);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mIsBuiltCertChainRootBuiltInRoot added in bug 1485652
  if (serVersionParsedToInt >= 5) {
    rv = objStream->ReadBoolean(&aIsBuiltCertChainRootBuiltInRoot);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mIsAcceptedEch added in bug 1678079
  if (serVersionParsedToInt >= 6) {
    bool isAcceptedEch;
    rv = objStream->ReadBoolean(&isAcceptedEch);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // If aCipherSuite is Some, the serialized TransportSecurityinfo had its
    // cipher suite and protocol information, which means it has this
    // information.
    if (aCipherSuite.isSome()) {
      aIsAcceptedEch.emplace(isAcceptedEch);
    }
  }

  // mPeerId added in bug 1738664
  if (serVersionParsedToInt >= 7) {
    rv = objStream->ReadCString(aPeerId);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (serVersionParsedToInt >= 9) {
    rv = objStream->ReadBoolean(&aMadeOCSPRequests);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = objStream->ReadBoolean(&aUsedPrivateDNS);
    if (NS_FAILED(rv)) {
      return rv;
    };
  }

  RefPtr<nsITransportSecurityInfo> securityInfo(new TransportSecurityInfo(
      aSecurityState, aErrorCode, std::move(aFailedCertChain), aServerCert,
      std::move(aSucceededCertChain), aCipherSuite, aKeaGroupName,
      aSignatureSchemeName, aProtocolVersion, aCertificateTransparencyStatus,
      aIsAcceptedEch, aIsDelegatedCredential, aOverridableErrorCategory,
      aMadeOCSPRequests, aUsedPrivateDNS, aIsEV, aNPNCompleted, aNegotiatedNPN,
      aResumed, aIsBuiltCertChainRootBuiltInRoot, aPeerId));
  securityInfo.forget(aResult);
  return NS_OK;
}

void TransportSecurityInfo::SerializeToIPC(IPC::MessageWriter* aWriter) {
  WriteParam(aWriter, mSecurityState);
  WriteParam(aWriter, mErrorCode);
  WriteParam(aWriter, mFailedCertChain);
  WriteParam(aWriter, mServerCert);
  WriteParam(aWriter, mSucceededCertChain);
  WriteParam(aWriter, mCipherSuite);
  WriteParam(aWriter, mKeaGroupName);
  WriteParam(aWriter, mSignatureSchemeName);
  WriteParam(aWriter, mProtocolVersion);
  WriteParam(aWriter, mCertificateTransparencyStatus);
  WriteParam(aWriter, mIsAcceptedEch);
  WriteParam(aWriter, mIsDelegatedCredential);
  WriteParam(aWriter, mOverridableErrorCategory);
  WriteParam(aWriter, mMadeOCSPRequests);
  WriteParam(aWriter, mUsedPrivateDNS);
  WriteParam(aWriter, mIsEV);
  WriteParam(aWriter, mNPNCompleted);
  WriteParam(aWriter, mNegotiatedNPN);
  WriteParam(aWriter, mResumed);
  WriteParam(aWriter, mIsBuiltCertChainRootBuiltInRoot);
  WriteParam(aWriter, mPeerId);
}

bool TransportSecurityInfo::DeserializeFromIPC(
    IPC::MessageReader* aReader, RefPtr<nsITransportSecurityInfo>* aResult) {
  uint32_t aSecurityState;
  PRErrorCode aErrorCode;
  nsTArray<RefPtr<nsIX509Cert>> aFailedCertChain;
  nsCOMPtr<nsIX509Cert> aServerCert;
  nsTArray<RefPtr<nsIX509Cert>> aSucceededCertChain;
  Maybe<uint16_t> aCipherSuite;
  Maybe<nsCString> aKeaGroupName;
  Maybe<nsCString> aSignatureSchemeName;
  Maybe<uint16_t> aProtocolVersion;
  uint16_t aCertificateTransparencyStatus;
  Maybe<bool> aIsAcceptedEch;
  Maybe<bool> aIsDelegatedCredential;
  Maybe<OverridableErrorCategory> aOverridableErrorCategory;
  bool aMadeOCSPRequests;
  bool aUsedPrivateDNS;
  Maybe<bool> aIsEV;
  bool aNPNCompleted;
  nsCString aNegotiatedNPN;
  bool aResumed;
  bool aIsBuiltCertChainRootBuiltInRoot;
  nsCString aPeerId;

  if (!ReadParam(aReader, &aSecurityState) ||
      !ReadParam(aReader, &aErrorCode) ||
      !ReadParam(aReader, &aFailedCertChain) ||
      !ReadParam(aReader, &aServerCert) ||
      !ReadParam(aReader, &aSucceededCertChain) ||
      !ReadParam(aReader, &aCipherSuite) ||
      !ReadParam(aReader, &aKeaGroupName) ||
      !ReadParam(aReader, &aSignatureSchemeName) ||
      !ReadParam(aReader, &aProtocolVersion) ||
      !ReadParam(aReader, &aCertificateTransparencyStatus) ||
      !ReadParam(aReader, &aIsAcceptedEch) ||
      !ReadParam(aReader, &aIsDelegatedCredential) ||
      !ReadParam(aReader, &aOverridableErrorCategory) ||
      !ReadParam(aReader, &aMadeOCSPRequests) ||
      !ReadParam(aReader, &aUsedPrivateDNS) || !ReadParam(aReader, &aIsEV) ||
      !ReadParam(aReader, &aNPNCompleted) ||
      !ReadParam(aReader, &aNegotiatedNPN) || !ReadParam(aReader, &aResumed) ||
      !ReadParam(aReader, &aIsBuiltCertChainRootBuiltInRoot) ||
      !ReadParam(aReader, &aPeerId)) {
    return false;
  }

  RefPtr<nsITransportSecurityInfo> securityInfo(new TransportSecurityInfo(
      aSecurityState, aErrorCode, std::move(aFailedCertChain), aServerCert,
      std::move(aSucceededCertChain), aCipherSuite, aKeaGroupName,
      aSignatureSchemeName, aProtocolVersion, aCertificateTransparencyStatus,
      aIsAcceptedEch, aIsDelegatedCredential, aOverridableErrorCategory,
      aMadeOCSPRequests, aUsedPrivateDNS, aIsEV, aNPNCompleted, aNegotiatedNPN,
      aResumed, aIsBuiltCertChainRootBuiltInRoot, aPeerId));
  *aResult = std::move(securityInfo);
  return true;
}

NS_IMETHODIMP
TransportSecurityInfo::GetFailedCertChain(
    nsTArray<RefPtr<nsIX509Cert>>& aFailedCertChain) {
  MOZ_ASSERT(aFailedCertChain.IsEmpty());
  if (!aFailedCertChain.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  aFailedCertChain.AppendElements(mFailedCertChain);
  return NS_OK;
}

NS_IMETHODIMP TransportSecurityInfo::GetServerCert(nsIX509Cert** aServerCert) {
  NS_ENSURE_ARG_POINTER(aServerCert);
  nsCOMPtr<nsIX509Cert> cert = mServerCert;
  cert.forget(aServerCert);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetSucceededCertChain(
    nsTArray<RefPtr<nsIX509Cert>>& aSucceededCertChain) {
  MOZ_ASSERT(aSucceededCertChain.IsEmpty());
  if (!aSucceededCertChain.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  aSucceededCertChain.AppendElements(mSucceededCertChain);
  return NS_OK;
}

NS_IMETHODIMP TransportSecurityInfo::GetIsBuiltCertChainRootBuiltInRoot(
    bool* aIsBuiltInRoot) {
  NS_ENSURE_ARG_POINTER(aIsBuiltInRoot);
  *aIsBuiltInRoot = mIsBuiltCertChainRootBuiltInRoot;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetCipherName(nsACString& aCipherName) {
  if (mCipherSuite.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(*mCipherSuite, &cipherInfo, sizeof(cipherInfo)) !=
      SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  aCipherName.Assign(cipherInfo.cipherSuiteName);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetKeyLength(uint32_t* aKeyLength) {
  NS_ENSURE_ARG_POINTER(aKeyLength);

  if (mCipherSuite.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(*mCipherSuite, &cipherInfo, sizeof(cipherInfo)) !=
      SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  *aKeyLength = cipherInfo.symKeyBits;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetSecretKeyLength(uint32_t* aSecretKeyLength) {
  NS_ENSURE_ARG_POINTER(aSecretKeyLength);

  if (mCipherSuite.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(*mCipherSuite, &cipherInfo, sizeof(cipherInfo)) !=
      SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  *aSecretKeyLength = cipherInfo.effectiveKeyBits;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetKeaGroupName(nsACString& aKeaGroupName) {
  if (mKeaGroupName.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aKeaGroupName.Assign(*mKeaGroupName);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetSignatureSchemeName(nsACString& aSignatureScheme) {
  if (mSignatureSchemeName.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aSignatureScheme.Assign(*mSignatureSchemeName);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetProtocolVersion(uint16_t* aProtocolVersion) {
  if (mProtocolVersion.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aProtocolVersion = *mProtocolVersion;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetCertificateTransparencyStatus(
    uint16_t* aCertificateTransparencyStatus) {
  NS_ENSURE_ARG_POINTER(aCertificateTransparencyStatus);

  *aCertificateTransparencyStatus = mCertificateTransparencyStatus;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetMadeOCSPRequests(bool* aMadeOCSPRequests) {
  *aMadeOCSPRequests = mMadeOCSPRequests;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetUsedPrivateDNS(bool* aUsedPrivateDNS) {
  *aUsedPrivateDNS = mUsedPrivateDNS;
  return NS_OK;
}

// static
uint16_t TransportSecurityInfo::ConvertCertificateTransparencyInfoToStatus(
    const mozilla::psm::CertificateTransparencyInfo& info) {
  using mozilla::ct::CTPolicyCompliance;

  if (!info.enabled) {
    // CT disabled.
    return nsITransportSecurityInfo::CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE;
  }

  switch (info.policyCompliance) {
    case CTPolicyCompliance::Compliant:
      return nsITransportSecurityInfo::
          CERTIFICATE_TRANSPARENCY_POLICY_COMPLIANT;
    case CTPolicyCompliance::NotEnoughScts:
      return nsITransportSecurityInfo ::
          CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS;
    case CTPolicyCompliance::NotDiverseScts:
      return nsITransportSecurityInfo ::
          CERTIFICATE_TRANSPARENCY_POLICY_NOT_DIVERSE_SCTS;
    case CTPolicyCompliance::Unknown:
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected CTPolicyCompliance type");
  }

  return nsITransportSecurityInfo::CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE;
}

NS_IMETHODIMP
TransportSecurityInfo::GetOverridableErrorCategory(
    OverridableErrorCategory* aOverridableErrorCategory) {
  NS_ENSURE_ARG_POINTER(aOverridableErrorCategory);

  if (mOverridableErrorCategory.isSome()) {
    *aOverridableErrorCategory = *mOverridableErrorCategory;
  } else {
    *aOverridableErrorCategory = OverridableErrorCategory::ERROR_UNSET;
  }
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetIsExtendedValidation(bool* aIsEV) {
  NS_ENSURE_ARG_POINTER(aIsEV);

  *aIsEV = false;
  // Never allow bad certs for EV, regardless of overrides.
  if (mOverridableErrorCategory.isSome()) {
    return NS_OK;
  }

  if (!mIsEV.isSome()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aIsEV = *mIsEV;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetIsAcceptedEch(bool* aIsAcceptedEch) {
  NS_ENSURE_ARG_POINTER(aIsAcceptedEch);

  if (mIsAcceptedEch.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aIsAcceptedEch = *mIsAcceptedEch;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetIsDelegatedCredential(bool* aIsDelegatedCredential) {
  NS_ENSURE_ARG_POINTER(aIsDelegatedCredential);

  if (mIsDelegatedCredential.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aIsDelegatedCredential = *mIsDelegatedCredential;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetNegotiatedNPN(nsACString& aNegotiatedNPN) {
  if (!mNPNCompleted) {
    return NS_ERROR_NOT_CONNECTED;
  }

  aNegotiatedNPN = mNegotiatedNPN;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetResumed(bool* aResumed) {
  NS_ENSURE_ARG_POINTER(aResumed);
  *aResumed = mResumed;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetPeerId(nsACString& aResult) {
  aResult.Assign(mPeerId);
  return NS_OK;
}

}  // namespace psm
}  // namespace mozilla

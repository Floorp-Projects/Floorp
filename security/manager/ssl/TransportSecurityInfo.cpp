/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransportSecurityInfo.h"

#include "PSMRunnable.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozpkix/pkixtypes.h"
#include "nsBase64Encoder.h"
#include "nsComponentManagerUtils.h"
#include "nsICertOverrideService.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIWebProgressListener.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "nsXULAppAPI.h"
#include "secerr.h"
#include "ssl.h"

// #define DEBUG_SSL_VERBOSE //Enable this define to get minimal
//  reports when doing SSL read/write

// #define DUMP_BUFFER  //Enable this define along with
//  DEBUG_SSL_VERBOSE to dump SSL
//  read/write buffer to a log.
//  Uses PR_LOG except on Mac where
//  we always write out to our own
//  file.

namespace mozilla {
namespace psm {

TransportSecurityInfo::TransportSecurityInfo()
    : mOverridableErrorCategory(OverridableErrorCategory::ERROR_UNSET),
      mIsEV(false),
      mHasIsEVStatus(false),
      mHaveCipherSuiteAndProtocol(false),
      mHaveCertErrorBits(false),
      mCanceled(false),
      mMutex("TransportSecurityInfo::mMutex"),
      mCipherSuite(0),
      mProtocolVersion(0),
      mCertificateTransparencyStatus(
          nsITransportSecurityInfo::CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE),
      mKeaGroup(),
      mSignatureSchemeName(),
      mIsAcceptedEch(false),
      mIsDelegatedCredential(false),
      mMadeOCSPRequests(false),
      mUsedPrivateDNS(false),
      mNPNCompleted(false),
      mResumed(false),
      mIsBuiltCertChainRootBuiltInRoot(false),
      mSecurityState(nsIWebProgressListener::STATE_IS_INSECURE),
      mErrorCode(0),
      mPort(0) {}

NS_IMPL_ISUPPORTS(TransportSecurityInfo, nsITransportSecurityInfo,
                  nsIInterfaceRequestor)

void TransportSecurityInfo::SetPreliminaryHandshakeInfo(
    const SSLChannelInfo& channelInfo, const SSLCipherSuiteInfo& cipherInfo) {
  MutexAutoLock lock(mMutex);
  mResumed = channelInfo.resumed;
  mCipherSuite = channelInfo.cipherSuite;
  mProtocolVersion = channelInfo.protocolVersion & 0xFF;
  mKeaGroup.Assign(getKeaGroupName(channelInfo.keaGroup));
  mSignatureSchemeName.Assign(getSignatureName(channelInfo.signatureScheme));
  mIsDelegatedCredential = channelInfo.peerDelegCred;
  mIsAcceptedEch = channelInfo.echAccepted;
  mHaveCipherSuiteAndProtocol = true;
}

void TransportSecurityInfo::SetHostName(const char* host) {
  MutexAutoLock lock(mMutex);

  mHostName.Assign(host);
}

void TransportSecurityInfo::SetPort(int32_t aPort) { mPort = aPort; }

void TransportSecurityInfo::SetOriginAttributes(
    const OriginAttributes& aOriginAttributes) {
  MutexAutoLock lock(mMutex);

  mOriginAttributes = aOriginAttributes;
}

// NB: GetErrorCode may be called before an error code is set (if ever). In that
// case, this returns (by pointer) 0, which is treated as a successful value.
NS_IMETHODIMP
TransportSecurityInfo::GetErrorCode(int32_t* state) {
  // We're in an inconsistent state if we think we've been canceled but no error
  // code was set or we haven't been canceled but an error code was set.
  MOZ_ASSERT(
      !((mCanceled && mErrorCode == 0) || (!mCanceled && mErrorCode != 0)));
  if ((mCanceled && mErrorCode == 0) || (!mCanceled && mErrorCode != 0)) {
    mCanceled = true;
    mErrorCode = SEC_ERROR_LIBRARY_FAILURE;
  }

  *state = mErrorCode;
  return NS_OK;
}

void TransportSecurityInfo::SetCanceled(PRErrorCode errorCode) {
  MOZ_ASSERT(errorCode != 0);
  if (errorCode == 0) {
    errorCode = SEC_ERROR_LIBRARY_FAILURE;
  }

  mErrorCode = errorCode;
  mCanceled = true;
}

bool TransportSecurityInfo::IsCanceled() { return mCanceled; }

NS_IMETHODIMP
TransportSecurityInfo::GetSecurityState(uint32_t* state) {
  *state = mSecurityState;
  return NS_OK;
}

void TransportSecurityInfo::SetSecurityState(uint32_t aState) {
  mSecurityState = aState;
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

NS_IMETHODIMP
TransportSecurityInfo::GetInterface(const nsIID& uuid, void** result) {
  if (!NS_IsMainThread()) {
    NS_ERROR("nsNSSSocketInfo::GetInterface called off the main thread");
    return NS_ERROR_NOT_SAME_THREAD;
  }
  MutexAutoLock lock(mMutex);

  nsresult rv;
  if (!mCallbacks) {
    nsCOMPtr<nsIInterfaceRequestor> ir = new PipUIContext();
    rv = ir->GetInterface(uuid, result);
  } else {
    rv = mCallbacks->GetInterface(uuid, result);
  }
  return rv;
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

// NB: Any updates (except disk-only fields) must be kept in sync with
//     |SerializeToIPC|.
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

  MutexAutoLock lock(mMutex);

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

  rv = objStream->Write16(mCipherSuite);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->Write16(mProtocolVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->Write32(mOverridableErrorCategory);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = objStream->WriteBoolean(mIsEV);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->WriteBoolean(mHasIsEVStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = objStream->WriteBoolean(mHaveCipherSuiteAndProtocol);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = objStream->WriteBoolean(mHaveCertErrorBits);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->Write16(mCertificateTransparencyStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->WriteStringZ(mKeaGroup.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = objStream->WriteStringZ(mSignatureSchemeName.get());
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

  rv = objStream->WriteBoolean(mIsDelegatedCredential);
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

  rv = objStream->WriteBoolean(mIsAcceptedEch);
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

#define CHILD_DIAGNOSTIC_ASSERT(condition, message)       \
  if (XRE_GetProcessType() == GeckoProcessType_Content) { \
    MOZ_DIAGNOSTIC_ASSERT(condition, message);            \
  }

nsresult TransportSecurityInfo::ReadOldOverridableErrorBits(
    nsIObjectInputStream* aStream,
    OverridableErrorCategory& aOverridableErrorCategory) {
  bool isDomainMismatch;
  nsresult rv = aStream->ReadBoolean(&isDomainMismatch);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  bool isNotValidAtThisTime;
  rv = aStream->ReadBoolean(&isNotValidAtThisTime);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  bool isUntrusted;
  rv = aStream->ReadBoolean(&isUntrusted);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
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
    uint16_t& aCipherSuite, uint16_t& aProtocolVersion,
    OverridableErrorCategory& aOverridableErrorCategory, bool& aIsEV,
    bool& aHasIsEVStatus, bool& aHaveCipherSuiteAndProtocol,
    bool& aHaveCertErrorBits, uint16_t& aCertificateTransparencyStatus,
    nsCString& aKeaGroup, nsCString& aSignatureSchemeName,
    nsTArray<RefPtr<nsIX509Cert>>& aSucceededCertChain) {
  bool nsISSLStatusPresent;
  nsresult rv = aStream->ReadBoolean(&nsISSLStatusPresent);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  if (!nsISSLStatusPresent) {
    return NS_OK;
  }
  // nsISSLStatus present.  Prepare to read elements.
  // Throw away cid, validate iid
  nsCID cid;
  nsIID iid;
  rv = aStream->ReadID(&cid);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadID(&iid);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);

  static const nsIID nsSSLStatusIID = {
      0xfa9ba95b,
      0xca3b,
      0x498a,
      {0xb8, 0x89, 0x7c, 0x79, 0xcf, 0x28, 0xfe, 0xe8}};
  if (!iid.Equals(nsSSLStatusIID)) {
    CHILD_DIAGNOSTIC_ASSERT(false, "Deserialization should not fail");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsISupports> cert;
  rv = aStream->ReadObject(true, getter_AddRefs(cert));
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);

  if (cert) {
    aServerCert = do_QueryInterface(cert);
    if (!aServerCert) {
      CHILD_DIAGNOSTIC_ASSERT(false, "Deserialization should not fail");
      return NS_NOINTERFACE;
    }
  }

  rv = aStream->Read16(&aCipherSuite);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);

  // The code below is a workaround to allow serializing new fields
  // while preserving binary compatibility with older streams. For more details
  // on the binary compatibility requirement, refer to bug 1248628.
  // Here, we take advantage of the fact that mProtocolVersion was originally
  // stored as a 16 bits integer, but the highest 8 bits were never used.
  // These bits are now used for stream versioning.
  uint16_t protocolVersionAndStreamFormatVersion;
  rv = aStream->Read16(&protocolVersionAndStreamFormatVersion);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  aProtocolVersion = protocolVersionAndStreamFormatVersion & 0xFF;
  const uint8_t streamFormatVersion =
      (protocolVersionAndStreamFormatVersion >> 8) & 0xFF;

  rv = ReadOldOverridableErrorBits(aStream, aOverridableErrorCategory);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&aIsEV);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->ReadBoolean(&aHasIsEVStatus);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&aHaveCipherSuiteAndProtocol);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&aHaveCertErrorBits);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);

  // Added in version 1 (see bug 1305289).
  if (streamFormatVersion >= 1) {
    rv = aStream->Read16(&aCertificateTransparencyStatus);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Added in version 2 (see bug 1304923).
  if (streamFormatVersion >= 2) {
    rv = aStream->ReadCString(aKeaGroup);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->ReadCString(aSignatureSchemeName);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Added in version 3 (see bug 1406856).
  if (streamFormatVersion >= 3) {
    rv = ReadCertList(aStream, aSucceededCertChain);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Read only to consume bytes from the stream.
    nsTArray<RefPtr<nsIX509Cert>> failedCertChain;
    rv = ReadCertList(aStream, failedCertChain);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
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
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  if (!nsIX509CertListPresent) {
    return NS_OK;
  }
  // nsIX509CertList present.  Prepare to read elements.
  // Throw away cid, validate iid
  nsCID cid;
  nsIID iid;
  rv = aStream->ReadID(&cid);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadID(&iid);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);

  static const nsIID nsIX509CertListIID = {
      0xae74cda5,
      0xcd2f,
      0x473f,
      {0x96, 0xf5, 0xf0, 0xb7, 0xff, 0xf6, 0x2c, 0x68}};

  if (!iid.Equals(nsIX509CertListIID)) {
    CHILD_DIAGNOSTIC_ASSERT(false, "Deserialization should not fail");
    return NS_ERROR_UNEXPECTED;
  }

  uint32_t certListSize;
  rv = aStream->Read32(&certListSize);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
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
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
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

// NB: Any updates (except disk-only fields) must be kept in sync with
//     |DeserializeFromIPC|.
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
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!id.Equals(kTransportSecurityInfoMagic)) {
    CHILD_DIAGNOSTIC_ASSERT(false, "Deserialization should not fail");
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<TransportSecurityInfo> securityInfo(new TransportSecurityInfo());
  MutexAutoLock guard(securityInfo->mMutex);
  rv = ReadUint32AndSetAtomicFieldHelper(objStream,
                                         securityInfo->mSecurityState);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsBrokenSecurity was removed in bug 748809
  uint32_t unusedSubRequestsBrokenSecurity;
  rv = objStream->Read32(&unusedSubRequestsBrokenSecurity);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsNoSecurity was removed in bug 748809
  uint32_t unusedSubRequestsNoSecurity;
  rv = objStream->Read32(&unusedSubRequestsNoSecurity);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint32_t errorCode;
  rv = objStream->Read32(&errorCode);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  // PRErrorCode will be a negative value
  securityInfo->mErrorCode = static_cast<PRErrorCode>(errorCode);
  // If mErrorCode is non-zero, SetCanceled was called on the
  // TransportSecurityInfo that was serialized.
  if (securityInfo->mErrorCode != 0) {
    securityInfo->mCanceled = true;
  }

  // Re-purpose mErrorMessageCached to represent serialization version
  // If string doesn't match exact version it will be treated as older
  // serialization.
  nsAutoString serVersion;
  rv = objStream->ReadString(serVersion);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
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
    OverridableErrorCategory overridableErrorCategory;
    bool isEV;
    bool hasIsEVStatus;
    bool haveCipherSuiteAndProtocol;
    bool haveCertErrorBits;
    rv = ReadSSLStatus(
        objStream, securityInfo->mServerCert, securityInfo->mCipherSuite,
        securityInfo->mProtocolVersion, overridableErrorCategory, isEV,
        hasIsEVStatus, haveCipherSuiteAndProtocol, haveCertErrorBits,
        securityInfo->mCertificateTransparencyStatus, securityInfo->mKeaGroup,
        securityInfo->mSignatureSchemeName, securityInfo->mSucceededCertChain);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    securityInfo->mOverridableErrorCategory = overridableErrorCategory;
    securityInfo->mIsEV = isEV;
    securityInfo->mHasIsEVStatus = hasIsEVStatus;
    securityInfo->mHaveCipherSuiteAndProtocol = haveCipherSuiteAndProtocol;
    securityInfo->mHaveCertErrorBits = haveCertErrorBits;
  } else {
    nsCOMPtr<nsISupports> cert;
    rv = NS_ReadOptionalObject(objStream, true, getter_AddRefs(cert));
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    if (cert != nullptr) {
      securityInfo->mServerCert = do_QueryInterface(cert);
      if (!securityInfo->mServerCert) {
        CHILD_DIAGNOSTIC_ASSERT(false, "Deserialization should not fail");
        return NS_NOINTERFACE;
      }
    }

    rv = objStream->Read16(&securityInfo->mCipherSuite);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = objStream->Read16(&securityInfo->mProtocolVersion);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    if (serVersionParsedToInt < 8) {
      OverridableErrorCategory overridableErrorCategory;
      rv = ReadOldOverridableErrorBits(objStream, overridableErrorCategory);
      NS_ENSURE_SUCCESS(rv, rv);
      securityInfo->mOverridableErrorCategory = overridableErrorCategory;
    } else {
      uint32_t overridableErrorCategory;
      rv = objStream->Read32(&overridableErrorCategory);
      CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                              "Deserialization should not fail");
      NS_ENSURE_SUCCESS(rv, rv);
      securityInfo->mOverridableErrorCategory =
          IntToOverridableErrorCategory(overridableErrorCategory);
    }
    rv = ReadBoolAndSetAtomicFieldHelper(objStream, securityInfo->mIsEV);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ReadBoolAndSetAtomicFieldHelper(objStream,
                                         securityInfo->mHasIsEVStatus);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ReadBoolAndSetAtomicFieldHelper(
        objStream, securityInfo->mHaveCipherSuiteAndProtocol);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ReadBoolAndSetAtomicFieldHelper(objStream,
                                         securityInfo->mHaveCertErrorBits);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = objStream->Read16(&securityInfo->mCertificateTransparencyStatus);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = objStream->ReadCString(securityInfo->mKeaGroup);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = objStream->ReadCString(securityInfo->mSignatureSchemeName);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    if (serVersionParsedToInt < 3) {
      // The old data structure of certList(nsIX509CertList) presents
      rv = ReadCertList(objStream, securityInfo->mSucceededCertChain);
      CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                              "Deserialization should not fail");
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      uint16_t certCount;
      rv = objStream->Read16(&certCount);
      CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                              "Deserialization should not fail");
      NS_ENSURE_SUCCESS(rv, rv);

      rv = ReadCertificatesFromStream(objStream, certCount,
                                      securityInfo->mSucceededCertChain);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  // END moved from nsISSLStatus
  if (serVersionParsedToInt < 3) {
    // The old data structure of certList(nsIX509CertList) presents
    rv = ReadCertList(objStream, securityInfo->mFailedCertChain);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    uint16_t certCount;
    rv = objStream->Read16(&certCount);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ReadCertificatesFromStream(objStream, certCount,
                                    securityInfo->mFailedCertChain);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // mIsDelegatedCredential added in bug 1562773
  if (serVersionParsedToInt >= 2) {
    rv = objStream->ReadBoolean(&securityInfo->mIsDelegatedCredential);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mNPNCompleted, mNegotiatedNPN, mResumed added in bug 1584104
  if (serVersionParsedToInt >= 4) {
    rv = objStream->ReadBoolean(&securityInfo->mNPNCompleted);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = objStream->ReadCString(securityInfo->mNegotiatedNPN);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = objStream->ReadBoolean(&securityInfo->mResumed);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mIsBuiltCertChainRootBuiltInRoot added in bug 1485652
  if (serVersionParsedToInt >= 5) {
    rv =
        objStream->ReadBoolean(&securityInfo->mIsBuiltCertChainRootBuiltInRoot);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mIsAcceptedEch added in bug 1678079
  if (serVersionParsedToInt >= 6) {
    rv = objStream->ReadBoolean(&securityInfo->mIsAcceptedEch);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mPeerId added in bug 1738664
  if (serVersionParsedToInt >= 7) {
    rv = objStream->ReadCString(securityInfo->mPeerId);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (serVersionParsedToInt >= 9) {
    rv = objStream->ReadBoolean(&securityInfo->mMadeOCSPRequests);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = objStream->ReadBoolean(&securityInfo->mUsedPrivateDNS);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    };
  }

  securityInfo.forget(aResult);
  return NS_OK;
}

#undef CHILD_DIAGNOSTIC_ASSERT

void TransportSecurityInfo::SerializeToIPC(IPC::MessageWriter* aWriter) {
  MutexAutoLock guard(mMutex);

  int32_t errorCode = static_cast<int32_t>(mErrorCode);

  WriteParam(aWriter, static_cast<uint32_t>(mSecurityState));
  WriteParam(aWriter, errorCode);
  WriteParam(aWriter, mServerCert);
  WriteParam(aWriter, mCipherSuite);
  WriteParam(aWriter, mProtocolVersion);
  WriteParam(aWriter, static_cast<uint32_t>(mOverridableErrorCategory));
  WriteParam(aWriter, static_cast<bool>(mIsEV));
  WriteParam(aWriter, static_cast<bool>(mHasIsEVStatus));
  WriteParam(aWriter, static_cast<bool>(mHaveCipherSuiteAndProtocol));
  WriteParam(aWriter, static_cast<bool>(mHaveCertErrorBits));
  WriteParam(aWriter, mCertificateTransparencyStatus);
  WriteParam(aWriter, mKeaGroup);
  WriteParam(aWriter, mSignatureSchemeName);
  WriteParam(aWriter, mSucceededCertChain);
  WriteParam(aWriter, mFailedCertChain);
  WriteParam(aWriter, mIsDelegatedCredential);
  WriteParam(aWriter, mNPNCompleted);
  WriteParam(aWriter, mNegotiatedNPN);
  WriteParam(aWriter, mResumed);
  WriteParam(aWriter, mIsBuiltCertChainRootBuiltInRoot);
  WriteParam(aWriter, mIsAcceptedEch);
  WriteParam(aWriter, mPeerId);
  WriteParam(aWriter, mMadeOCSPRequests);
  WriteParam(aWriter, mUsedPrivateDNS);
}

bool TransportSecurityInfo::DeserializeFromIPC(
    IPC::MessageReader* aReader, RefPtr<nsITransportSecurityInfo>* aResult) {
  RefPtr<TransportSecurityInfo> securityInfo(new TransportSecurityInfo());
  MutexAutoLock guard(securityInfo->mMutex);

  int32_t errorCode = 0;
  uint32_t overridableErrorCategory;

  if (!ReadParamAtomicHelper(aReader, securityInfo->mSecurityState) ||
      !ReadParam(aReader, &errorCode) ||
      !ReadParam(aReader, &securityInfo->mServerCert) ||
      !ReadParam(aReader, &securityInfo->mCipherSuite) ||
      !ReadParam(aReader, &securityInfo->mProtocolVersion) ||
      !ReadParam(aReader, &overridableErrorCategory) ||
      !ReadParamAtomicHelper(aReader, securityInfo->mIsEV) ||
      !ReadParamAtomicHelper(aReader, securityInfo->mHasIsEVStatus) ||
      !ReadParamAtomicHelper(aReader,
                             securityInfo->mHaveCipherSuiteAndProtocol) ||
      !ReadParamAtomicHelper(aReader, securityInfo->mHaveCertErrorBits) ||
      !ReadParam(aReader, &securityInfo->mCertificateTransparencyStatus) ||
      !ReadParam(aReader, &securityInfo->mKeaGroup) ||
      !ReadParam(aReader, &securityInfo->mSignatureSchemeName) ||
      !ReadParam(aReader, &securityInfo->mSucceededCertChain) ||
      !ReadParam(aReader, &securityInfo->mFailedCertChain) ||
      !ReadParam(aReader, &securityInfo->mIsDelegatedCredential) ||
      !ReadParam(aReader, &securityInfo->mNPNCompleted) ||
      !ReadParam(aReader, &securityInfo->mNegotiatedNPN) ||
      !ReadParam(aReader, &securityInfo->mResumed) ||
      !ReadParam(aReader, &securityInfo->mIsBuiltCertChainRootBuiltInRoot) ||
      !ReadParam(aReader, &securityInfo->mIsAcceptedEch) ||
      !ReadParam(aReader, &securityInfo->mPeerId) ||
      !ReadParam(aReader, &securityInfo->mMadeOCSPRequests) ||
      !ReadParam(aReader, &securityInfo->mUsedPrivateDNS)) {
    return false;
  }

  securityInfo->mErrorCode = static_cast<PRErrorCode>(errorCode);
  if (securityInfo->mErrorCode != 0) {
    securityInfo->mCanceled = true;
  }
  securityInfo->mOverridableErrorCategory =
      IntToOverridableErrorCategory(overridableErrorCategory);

  *aResult = std::move(securityInfo);
  return true;
}

void TransportSecurityInfo::SetStatusErrorBits(
    const nsCOMPtr<nsIX509Cert>& cert,
    OverridableErrorCategory overridableErrorCategory) {
  SetServerCert(cert, EVStatus::NotEV);

  mHaveCertErrorBits = true;
  mOverridableErrorCategory = overridableErrorCategory;
}

NS_IMETHODIMP
TransportSecurityInfo::GetFailedCertChain(
    nsTArray<RefPtr<nsIX509Cert>>& aFailedCertChain) {
  MOZ_ASSERT(aFailedCertChain.IsEmpty());
  MutexAutoLock lock(mMutex);
  if (!aFailedCertChain.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  aFailedCertChain.AppendElements(mFailedCertChain);
  return NS_OK;
}

static nsresult CreateCertChain(nsTArray<RefPtr<nsIX509Cert>>& aOutput,
                                nsTArray<nsTArray<uint8_t>>&& aCertList) {
  nsTArray<nsTArray<uint8_t>> certList = std::move(aCertList);
  aOutput.Clear();
  for (auto& certBytes : certList) {
    RefPtr<nsIX509Cert> cert = new nsNSSCertificate(std::move(certBytes));
    aOutput.AppendElement(cert);
  }
  return NS_OK;
}

nsresult TransportSecurityInfo::SetFailedCertChain(
    nsTArray<nsTArray<uint8_t>>&& aCertList) {
  MutexAutoLock lock(mMutex);

  return CreateCertChain(mFailedCertChain, std::move(aCertList));
}

NS_IMETHODIMP TransportSecurityInfo::GetServerCert(nsIX509Cert** aServerCert) {
  NS_ENSURE_ARG_POINTER(aServerCert);
  MutexAutoLock lock(mMutex);

  nsCOMPtr<nsIX509Cert> cert = mServerCert;
  cert.forget(aServerCert);
  return NS_OK;
}

void TransportSecurityInfo::SetServerCert(
    const nsCOMPtr<nsIX509Cert>& aServerCert, EVStatus aEVStatus) {
  MOZ_ASSERT(aServerCert);
  MutexAutoLock lock(mMutex);

  mServerCert = aServerCert;
  mIsEV = (aEVStatus == EVStatus::EV);
  mHasIsEVStatus = true;
}

NS_IMETHODIMP
TransportSecurityInfo::GetSucceededCertChain(
    nsTArray<RefPtr<nsIX509Cert>>& aSucceededCertChain) {
  MOZ_ASSERT(aSucceededCertChain.IsEmpty());
  MutexAutoLock lock(mMutex);
  if (!aSucceededCertChain.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }
  aSucceededCertChain.AppendElements(mSucceededCertChain);
  return NS_OK;
}

nsresult TransportSecurityInfo::SetSucceededCertChain(
    nsTArray<nsTArray<uint8_t>>&& aCertList) {
  MutexAutoLock lock(mMutex);
  return CreateCertChain(mSucceededCertChain, std::move(aCertList));
}

NS_IMETHODIMP TransportSecurityInfo::SetIsBuiltCertChainRootBuiltInRoot(
    bool aIsBuiltInRoot) {
  MutexAutoLock lock(mMutex);
  mIsBuiltCertChainRootBuiltInRoot = aIsBuiltInRoot;
  return NS_OK;
}

NS_IMETHODIMP TransportSecurityInfo::GetIsBuiltCertChainRootBuiltInRoot(
    bool* aIsBuiltInRoot) {
  NS_ENSURE_ARG_POINTER(aIsBuiltInRoot);
  MutexAutoLock lock(mMutex);
  *aIsBuiltInRoot = mIsBuiltCertChainRootBuiltInRoot;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetCipherName(nsACString& aCipherName) {
  MutexAutoLock lock(mMutex);

  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(mCipherSuite, &cipherInfo, sizeof(cipherInfo)) !=
      SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  aCipherName.Assign(cipherInfo.cipherSuiteName);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetKeyLength(uint32_t* aKeyLength) {
  NS_ENSURE_ARG_POINTER(aKeyLength);
  MutexAutoLock lock(mMutex);
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(mCipherSuite, &cipherInfo, sizeof(cipherInfo)) !=
      SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  *aKeyLength = cipherInfo.symKeyBits;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetSecretKeyLength(uint32_t* aSecretKeyLength) {
  NS_ENSURE_ARG_POINTER(aSecretKeyLength);
  MutexAutoLock lock(mMutex);
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(mCipherSuite, &cipherInfo, sizeof(cipherInfo)) !=
      SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  *aSecretKeyLength = cipherInfo.effectiveKeyBits;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetKeaGroupName(nsACString& aKeaGroup) {
  MutexAutoLock lock(mMutex);

  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aKeaGroup.Assign(mKeaGroup);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetSignatureSchemeName(nsACString& aSignatureScheme) {
  MutexAutoLock lock(mMutex);

  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aSignatureScheme.Assign(mSignatureSchemeName);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetProtocolVersion(uint16_t* aProtocolVersion) {
  MutexAutoLock lock(mMutex);

  NS_ENSURE_ARG_POINTER(aProtocolVersion);
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aProtocolVersion = mProtocolVersion;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetCertificateTransparencyStatus(
    uint16_t* aCertificateTransparencyStatus) {
  NS_ENSURE_ARG_POINTER(aCertificateTransparencyStatus);
  MutexAutoLock lock(mMutex);

  *aCertificateTransparencyStatus = mCertificateTransparencyStatus;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetMadeOCSPRequests(bool* aMadeOCSPRequests) {
  MutexAutoLock lock(mMutex);

  *aMadeOCSPRequests = mMadeOCSPRequests;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetUsedPrivateDNS(bool* aUsedPrivateDNS) {
  MutexAutoLock lock(mMutex);
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
  if (mHaveCertErrorBits) {
    *aOverridableErrorCategory = mOverridableErrorCategory;
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
TransportSecurityInfo::GetIsAcceptedEch(bool* aIsAcceptedEch) {
  NS_ENSURE_ARG_POINTER(aIsAcceptedEch);
  MutexAutoLock lock(mMutex);
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aIsAcceptedEch = mIsAcceptedEch;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetIsDelegatedCredential(bool* aIsDelegCred) {
  NS_ENSURE_ARG_POINTER(aIsDelegCred);
  MutexAutoLock lock(mMutex);
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aIsDelegCred = mIsDelegatedCredential;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetNegotiatedNPN(nsACString& aNegotiatedNPN) {
  MutexAutoLock lock(mMutex);

  if (!mNPNCompleted) {
    return NS_ERROR_NOT_CONNECTED;
  }

  aNegotiatedNPN = mNegotiatedNPN;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetResumed(bool* aResumed) {
  MutexAutoLock lock(mMutex);
  NS_ENSURE_ARG_POINTER(aResumed);
  *aResumed = mResumed;
  return NS_OK;
}

void TransportSecurityInfo::SetResumed(bool aResumed) {
  MutexAutoLock lock(mMutex);
  mResumed = aResumed;
}

NS_IMETHODIMP
TransportSecurityInfo::GetPeerId(nsACString& aResult) {
  MutexAutoLock lock(mMutex);
  aResult.Assign(mPeerId);
  return NS_OK;
}

}  // namespace psm
}  // namespace mozilla

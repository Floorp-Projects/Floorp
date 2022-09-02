/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransportSecurityInfo.h"

#include "PSMRunnable.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Casting.h"
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
#include "nsXULAppAPI.h"
#include "mozpkix/pkixtypes.h"
#include "secerr.h"
#include "ssl.h"

//#define DEBUG_SSL_VERBOSE //Enable this define to get minimal
// reports when doing SSL read/write

//#define DUMP_BUFFER  //Enable this define along with
// DEBUG_SSL_VERBOSE to dump SSL
// read/write buffer to a log.
// Uses PR_LOG except on Mac where
// we always write out to our own
// file.

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
                  nsIInterfaceRequestor, nsISerializable, nsIClassInfo)

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
TransportSecurityInfo::Write(nsIObjectOutputStream* aStream) {
  nsresult rv = aStream->WriteID(kTransportSecurityInfoMagic);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MutexAutoLock lock(mMutex);

  rv = aStream->Write32(mSecurityState);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsBrokenSecurity was removed in bug 748809
  rv = aStream->Write32(0);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsNoSecurity was removed in bug 748809
  rv = aStream->Write32(0);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = aStream->Write32(static_cast<uint32_t>(mErrorCode));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Re-purpose mErrorMessageCached to represent serialization version
  // If string doesn't match exact version it will be treated as older
  // serialization.
  rv = aStream->WriteWStringZ(NS_ConvertUTF8toUTF16("9").get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  // moved from nsISSLStatus
  rv = NS_WriteOptionalCompoundObject(aStream, mServerCert,
                                      NS_GET_IID(nsIX509Cert), true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->Write16(mCipherSuite);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->Write16(mProtocolVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->Write32(mOverridableErrorCategory);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mIsEV);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteBoolean(mHasIsEVStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mHaveCipherSuiteAndProtocol);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->WriteBoolean(mHaveCertErrorBits);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->Write16(mCertificateTransparencyStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteStringZ(mKeaGroup.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteStringZ(mSignatureSchemeName.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->Write16(mSucceededCertChain.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  for (const auto& cert : mSucceededCertChain) {
    rv = aStream->WriteCompoundObject(cert, NS_GET_IID(nsIX509Cert), true);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // END moved from nsISSLStatus
  rv = aStream->Write16(mFailedCertChain.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  for (const auto& cert : mFailedCertChain) {
    rv = aStream->WriteCompoundObject(cert, NS_GET_IID(nsIX509Cert), true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aStream->WriteBoolean(mIsDelegatedCredential);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteBoolean(mNPNCompleted);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteStringZ(mNegotiatedNPN.get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteBoolean(mResumed);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteBoolean(mIsBuiltCertChainRootBuiltInRoot);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteBoolean(mIsAcceptedEch);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteStringZ(mPeerId.get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteBoolean(mMadeOCSPRequests);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aStream->WriteBoolean(mUsedPrivateDNS);
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
    nsIObjectInputStream* aStream, MutexAutoLock& aProofOfLock) {
  mMutex.AssertCurrentThreadOwns();

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
    mOverridableErrorCategory =
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TRUST;
  } else if (isDomainMismatch) {
    mOverridableErrorCategory =
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_DOMAIN;
  } else if (isNotValidAtThisTime) {
    mOverridableErrorCategory =
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_TIME;
  } else {
    mOverridableErrorCategory =
        nsITransportSecurityInfo::OverridableErrorCategory::ERROR_UNSET;
  }

  return NS_OK;
}

// This is for backward compatibility to be able to read nsISSLStatus
// serialized object.
nsresult TransportSecurityInfo::ReadSSLStatus(nsIObjectInputStream* aStream,
                                              MutexAutoLock& aProofOfLock) {
  mMutex.AssertCurrentThreadOwns();

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
    mServerCert = do_QueryInterface(cert);
    if (!mServerCert) {
      CHILD_DIAGNOSTIC_ASSERT(false, "Deserialization should not fail");
      return NS_NOINTERFACE;
    }
  }

  rv = aStream->Read16(&mCipherSuite);
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
  mProtocolVersion = protocolVersionAndStreamFormatVersion & 0xFF;
  const uint8_t streamFormatVersion =
      (protocolVersionAndStreamFormatVersion >> 8) & 0xFF;

  rv = ReadOldOverridableErrorBits(aStream, aProofOfLock);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ReadBoolAndSetAtomicFieldHelper(aStream, mIsEV);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ReadBoolAndSetAtomicFieldHelper(aStream, mHasIsEVStatus);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ReadBoolAndSetAtomicFieldHelper(aStream, mHaveCipherSuiteAndProtocol);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ReadBoolAndSetAtomicFieldHelper(aStream, mHaveCertErrorBits);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);

  // Added in version 1 (see bug 1305289).
  if (streamFormatVersion >= 1) {
    rv = aStream->Read16(&mCertificateTransparencyStatus);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Added in version 2 (see bug 1304923).
  if (streamFormatVersion >= 2) {
    rv = aStream->ReadCString(mKeaGroup);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->ReadCString(mSignatureSchemeName);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Added in version 3 (see bug 1406856).
  if (streamFormatVersion >= 3) {
    rv = ReadCertList(aStream, mSucceededCertChain, aProofOfLock);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Read only to consume bytes from the stream.
    nsTArray<RefPtr<nsIX509Cert>> failedCertChain;
    rv = ReadCertList(aStream, failedCertChain, aProofOfLock);
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
    nsIObjectInputStream* aStream, nsTArray<RefPtr<nsIX509Cert>>& aCertList,
    MutexAutoLock& aProofOfLock) {
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

  return ReadCertificatesFromStream(aStream, certListSize, aCertList,
                                    aProofOfLock);
}

nsresult TransportSecurityInfo::ReadCertificatesFromStream(
    nsIObjectInputStream* aStream, uint32_t aSize,
    nsTArray<RefPtr<nsIX509Cert>>& aCertList, MutexAutoLock& aProofOfLock) {
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
NS_IMETHODIMP
TransportSecurityInfo::Read(nsIObjectInputStream* aStream) {
  nsID id;
  nsresult rv = aStream->ReadID(&id);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!id.Equals(kTransportSecurityInfoMagic)) {
    CHILD_DIAGNOSTIC_ASSERT(false, "Deserialization should not fail");
    return NS_ERROR_UNEXPECTED;
  }

  MutexAutoLock lock(mMutex);

  rv = ReadUint32AndSetAtomicFieldHelper(aStream, mSecurityState);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsBrokenSecurity was removed in bug 748809
  uint32_t unusedSubRequestsBrokenSecurity;
  rv = aStream->Read32(&unusedSubRequestsBrokenSecurity);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  // mSubRequestsNoSecurity was removed in bug 748809
  uint32_t unusedSubRequestsNoSecurity;
  rv = aStream->Read32(&unusedSubRequestsNoSecurity);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint32_t errorCode;
  rv = aStream->Read32(&errorCode);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  // PRErrorCode will be a negative value
  mErrorCode = static_cast<PRErrorCode>(errorCode);
  // If mErrorCode is non-zero, SetCanceled was called on the
  // TransportSecurityInfo that was serialized.
  if (mErrorCode != 0) {
    mCanceled = true;
  }

  // Re-purpose mErrorMessageCached to represent serialization version
  // If string doesn't match exact version it will be treated as older
  // serialization.
  nsAutoString serVersion;
  rv = aStream->ReadString(serVersion);
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
    rv = ReadSSLStatus(aStream, lock);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsCOMPtr<nsISupports> cert;
    rv = NS_ReadOptionalObject(aStream, true, getter_AddRefs(cert));
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    if (cert != nullptr) {
      mServerCert = do_QueryInterface(cert);
      if (!mServerCert) {
        CHILD_DIAGNOSTIC_ASSERT(false, "Deserialization should not fail");
        return NS_NOINTERFACE;
      }
    }

    rv = aStream->Read16(&mCipherSuite);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->Read16(&mProtocolVersion);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    if (serVersionParsedToInt < 8) {
      rv = ReadOldOverridableErrorBits(aStream, lock);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      uint32_t overridableErrorCategory;
      rv = aStream->Read32(&overridableErrorCategory);
      CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                              "Deserialization should not fail");
      NS_ENSURE_SUCCESS(rv, rv);
      mOverridableErrorCategory =
          IntToOverridableErrorCategory(overridableErrorCategory);
    }
    rv = ReadBoolAndSetAtomicFieldHelper(aStream, mIsEV);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ReadBoolAndSetAtomicFieldHelper(aStream, mHasIsEVStatus);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ReadBoolAndSetAtomicFieldHelper(aStream, mHaveCipherSuiteAndProtocol);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ReadBoolAndSetAtomicFieldHelper(aStream, mHaveCertErrorBits);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->Read16(&mCertificateTransparencyStatus);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->ReadCString(mKeaGroup);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->ReadCString(mSignatureSchemeName);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    if (serVersionParsedToInt < 3) {
      // The old data structure of certList(nsIX509CertList) presents
      rv = ReadCertList(aStream, mSucceededCertChain, lock);
      CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                              "Deserialization should not fail");
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      uint16_t certCount;
      rv = aStream->Read16(&certCount);
      CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                              "Deserialization should not fail");
      NS_ENSURE_SUCCESS(rv, rv);

      rv = ReadCertificatesFromStream(aStream, certCount, mSucceededCertChain,
                                      lock);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  // END moved from nsISSLStatus
  if (serVersionParsedToInt < 3) {
    // The old data structure of certList(nsIX509CertList) presents
    rv = ReadCertList(aStream, mFailedCertChain, lock);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    uint16_t certCount;
    rv = aStream->Read16(&certCount);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ReadCertificatesFromStream(aStream, certCount, mFailedCertChain, lock);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // mIsDelegatedCredential added in bug 1562773
  if (serVersionParsedToInt >= 2) {
    rv = aStream->ReadBoolean(&mIsDelegatedCredential);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mNPNCompleted, mNegotiatedNPN, mResumed added in bug 1584104
  if (serVersionParsedToInt >= 4) {
    rv = aStream->ReadBoolean(&mNPNCompleted);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = aStream->ReadCString(mNegotiatedNPN);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = aStream->ReadBoolean(&mResumed);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mIsBuiltCertChainRootBuiltInRoot added in bug 1485652
  if (serVersionParsedToInt >= 5) {
    rv = aStream->ReadBoolean(&mIsBuiltCertChainRootBuiltInRoot);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mIsAcceptedEch added in bug 1678079
  if (serVersionParsedToInt >= 6) {
    rv = aStream->ReadBoolean(&mIsAcceptedEch);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mPeerId added in bug 1738664
  if (serVersionParsedToInt >= 7) {
    rv = aStream->ReadCString(mPeerId);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (serVersionParsedToInt >= 9) {
    rv = aStream->ReadBoolean(&mMadeOCSPRequests);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = aStream->ReadBoolean(&mUsedPrivateDNS);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    };
  }

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

bool TransportSecurityInfo::DeserializeFromIPC(IPC::MessageReader* aReader) {
  MutexAutoLock guard(mMutex);

  int32_t errorCode = 0;
  uint32_t overridableErrorCategory;

  if (!ReadParamAtomicHelper(aReader, mSecurityState) ||
      !ReadParam(aReader, &errorCode) || !ReadParam(aReader, &mServerCert) ||
      !ReadParam(aReader, &mCipherSuite) ||
      !ReadParam(aReader, &mProtocolVersion) ||
      !ReadParam(aReader, &overridableErrorCategory) ||
      !ReadParamAtomicHelper(aReader, mIsEV) ||
      !ReadParamAtomicHelper(aReader, mHasIsEVStatus) ||
      !ReadParamAtomicHelper(aReader, mHaveCipherSuiteAndProtocol) ||
      !ReadParamAtomicHelper(aReader, mHaveCertErrorBits) ||
      !ReadParam(aReader, &mCertificateTransparencyStatus) ||
      !ReadParam(aReader, &mKeaGroup) ||
      !ReadParam(aReader, &mSignatureSchemeName) ||
      !ReadParam(aReader, &mSucceededCertChain) ||
      !ReadParam(aReader, &mFailedCertChain) ||
      !ReadParam(aReader, &mIsDelegatedCredential) ||
      !ReadParam(aReader, &mNPNCompleted) ||
      !ReadParam(aReader, &mNegotiatedNPN) || !ReadParam(aReader, &mResumed) ||
      !ReadParam(aReader, &mIsBuiltCertChainRootBuiltInRoot) ||
      !ReadParam(aReader, &mIsAcceptedEch) || !ReadParam(aReader, &mPeerId) ||
      !ReadParam(aReader, &mMadeOCSPRequests) ||
      !ReadParam(aReader, &mUsedPrivateDNS)) {
    return false;
  }

  mErrorCode = static_cast<PRErrorCode>(errorCode);
  if (mErrorCode != 0) {
    mCanceled = true;
  }
  mOverridableErrorCategory =
      IntToOverridableErrorCategory(overridableErrorCategory);

  return true;
}

NS_IMETHODIMP
TransportSecurityInfo::GetInterfaces(nsTArray<nsIID>& array) {
  array.Clear();
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetScriptableHelper(nsIXPCScriptable** _retval) {
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetContractID(nsACString& aContractID) {
  aContractID.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetClassDescription(nsACString& aClassDescription) {
  aClassDescription.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetClassID(nsCID** aClassID) {
  *aClassID = (nsCID*)moz_xmalloc(sizeof(nsCID));
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
TransportSecurityInfo::GetFlags(uint32_t* aFlags) {
  *aFlags = 0;
  return NS_OK;
}

static NS_DEFINE_CID(kNSSSocketInfoCID, TRANSPORTSECURITYINFO_CID);

NS_IMETHODIMP
TransportSecurityInfo::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  *aClassIDNoAlloc = kNSSSocketInfoCID;
  return NS_OK;
}

// RememberCertErrorsTable

/*static*/
RememberCertErrorsTable* RememberCertErrorsTable::sInstance = nullptr;

RememberCertErrorsTable::RememberCertErrorsTable()
    : mErrorHosts(), mMutex("RememberCertErrorsTable::mMutex") {}

static nsresult GetHostPortKey(TransportSecurityInfo* infoObject,
                               /*out*/ nsCString& result) {
  MOZ_ASSERT(infoObject);
  NS_ENSURE_ARG(infoObject);

  result.Truncate();

  result.Assign(infoObject->GetHostName());
  result.Append(':');
  result.AppendInt(infoObject->GetPort());

  return NS_OK;
}

void RememberCertErrorsTable::RememberCertHasError(
    TransportSecurityInfo* infoObject, SECStatus certVerificationResult) {
  nsresult rv;

  nsAutoCString hostPortKey;
  rv = GetHostPortKey(infoObject, hostPortKey);
  if (NS_FAILED(rv)) {
    return;
  }

  if (certVerificationResult != SECSuccess) {
    MOZ_ASSERT(infoObject->mHaveCertErrorBits,
               "Must have error bits when remembering flags");
    if (!infoObject->mHaveCertErrorBits) {
      return;
    }

    MutexAutoLock lock(mMutex);
    mErrorHosts.InsertOrUpdate(hostPortKey,
                               infoObject->mOverridableErrorCategory);
  } else {
    MutexAutoLock lock(mMutex);
    mErrorHosts.Remove(hostPortKey);
  }
}

void RememberCertErrorsTable::LookupCertErrorBits(
    TransportSecurityInfo* infoObject) {
  // Get remembered error bits from our cache, because of SSL session caching
  // the NSS library potentially hasn't notified us for this socket.
  if (infoObject->mHaveCertErrorBits) {
    // Rather do not modify bits if already set earlier
    return;
  }

  nsresult rv;

  nsAutoCString hostPortKey;
  rv = GetHostPortKey(infoObject, hostPortKey);
  if (NS_FAILED(rv)) {
    return;
  }

  nsITransportSecurityInfo::OverridableErrorCategory overridableErrorCategory;
  {
    MutexAutoLock lock(mMutex);
    if (!mErrorHosts.Get(hostPortKey, &overridableErrorCategory)) {
      // No record was found, this host had no cert errors
      return;
    }
  }

  // This host had cert errors, update the bits correctly
  infoObject->mHaveCertErrorBits = true;
  infoObject->mOverridableErrorCategory = overridableErrorCategory;
}

void TransportSecurityInfo::SetStatusErrorBits(
    const nsCOMPtr<nsIX509Cert>& cert,
    OverridableErrorCategory overridableErrorCategory) {
  SetServerCert(cert, EVStatus::NotEV);

  mHaveCertErrorBits = true;
  mOverridableErrorCategory = overridableErrorCategory;

  RememberCertErrorsTable::GetInstance().RememberCertHasError(this, SECFailure);
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

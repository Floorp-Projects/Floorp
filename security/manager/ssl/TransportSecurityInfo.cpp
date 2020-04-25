/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransportSecurityInfo.h"

#include "DateTimeFormat.h"
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
    : mCipherSuite(0),
      mProtocolVersion(0),
      mCertificateTransparencyStatus(
          nsITransportSecurityInfo::CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE),
      mKeaGroup(),
      mSignatureSchemeName(),
      mIsDelegatedCredential(false),
      mIsDomainMismatch(false),
      mIsNotValidAtThisTime(false),
      mIsUntrusted(false),
      mIsEV(false),
      mHasIsEVStatus(false),
      mHaveCipherSuiteAndProtocol(false),
      mHaveCertErrorBits(false),
      mCanceled(false),
      mMutex("TransportSecurityInfo::mMutex"),
      mNPNCompleted(false),
      mResumed(false),
      mIsBuiltCertChainRootBuiltInRoot(false),
      mSecurityState(nsIWebProgressListener::STATE_IS_INSECURE),
      mErrorCode(0),
      mPort(0) {}

NS_IMPL_ISUPPORTS(TransportSecurityInfo, nsITransportSecurityInfo,
                  nsIInterfaceRequestor, nsISerializable, nsIClassInfo)

void TransportSecurityInfo::SetHostName(const char* host) {
  mHostName.Assign(host);
}

void TransportSecurityInfo::SetPort(int32_t aPort) { mPort = aPort; }

void TransportSecurityInfo::SetOriginAttributes(
    const OriginAttributes& aOriginAttributes) {
  mOriginAttributes = aOriginAttributes;
}

// NB: GetErrorCode may be called before an error code is set (if ever). In that
// case, this returns (by pointer) 0, which is treated as a successful value.
NS_IMETHODIMP
TransportSecurityInfo::GetErrorCode(int32_t* state) {
  MutexAutoLock lock(mMutex);

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

  MutexAutoLock lock(mMutex);
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
  MutexAutoLock lock(mMutex);

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
  rv = aStream->WriteWStringZ(NS_ConvertUTF8toUTF16("5").get());
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

  rv = aStream->Write16(mCertificateTransparencyStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteStringZ(mKeaGroup.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteStringZ(mSignatureSchemeName.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->Write16(mSucceededCertChain.Length());
  NS_ENSURE_SUCCESS(rv, rv);

  for (const auto& cert : mSucceededCertChain) {
    nsCOMPtr<nsISerializable> serializableCert = do_QueryInterface(cert);
    rv = aStream->WriteCompoundObject(serializableCert, NS_GET_IID(nsIX509Cert),
                                      true);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // END moved from nsISSLStatus
  rv = aStream->Write16(mFailedCertChain.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  for (const auto& cert : mFailedCertChain) {
    nsCOMPtr<nsISerializable> serializableCert = do_QueryInterface(cert);
    rv = aStream->WriteCompoundObject(serializableCert, NS_GET_IID(nsIX509Cert),
                                      true);
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

  return NS_OK;
}

#define CHILD_DIAGNOSTIC_ASSERT(condition, message)       \
  if (XRE_GetProcessType() == GeckoProcessType_Content) { \
    MOZ_DIAGNOSTIC_ASSERT(condition, message);            \
  }

// This is for backward compatibility to be able to read nsISSLStatus
// serialized object.
nsresult TransportSecurityInfo::ReadSSLStatus(nsIObjectInputStream* aStream) {
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

  rv = aStream->ReadBoolean(&mIsDomainMismatch);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mIsNotValidAtThisTime);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mIsUntrusted);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mIsEV);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->ReadBoolean(&mHasIsEVStatus);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mHaveCipherSuiteAndProtocol);
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aStream->ReadBoolean(&mHaveCertErrorBits);
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
    rv = ReadCertList(aStream, mSucceededCertChain);
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

  rv = aStream->Read32(&mSecurityState);
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

  // moved from nsISSLStatus
  if (!serVersion.EqualsASCII("1") && !serVersion.EqualsASCII("2") &&
      !serVersion.EqualsASCII("3") && !serVersion.EqualsASCII("4") &&
      !serVersion.EqualsASCII("5")) {
    // nsISSLStatus may be present
    rv = ReadSSLStatus(aStream);
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

    rv = aStream->ReadBoolean(&mIsDomainMismatch);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aStream->ReadBoolean(&mIsNotValidAtThisTime);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aStream->ReadBoolean(&mIsUntrusted);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aStream->ReadBoolean(&mIsEV);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->ReadBoolean(&mHasIsEVStatus);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aStream->ReadBoolean(&mHaveCipherSuiteAndProtocol);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aStream->ReadBoolean(&mHaveCertErrorBits);
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

    if (!serVersion.EqualsASCII("3") && !serVersion.EqualsASCII("4") &&
        !serVersion.EqualsASCII("5")) {
      // The old data structure of certList(nsIX509CertList) presents
      rv = ReadCertList(aStream, mSucceededCertChain);
      CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                              "Deserialization should not fail");
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      uint16_t certCount;
      rv = aStream->Read16(&certCount);
      CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                              "Deserialization should not fail");
      NS_ENSURE_SUCCESS(rv, rv);

      rv = ReadCertificatesFromStream(aStream, certCount, mSucceededCertChain);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  // END moved from nsISSLStatus
  if (!serVersion.EqualsASCII("3") && !serVersion.EqualsASCII("4") &&
      !serVersion.EqualsASCII("5")) {
    // The old data structure of certList(nsIX509CertList) presents
    rv = ReadCertList(aStream, mFailedCertChain);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    uint16_t certCount;
    rv = aStream->Read16(&certCount);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ReadCertificatesFromStream(aStream, certCount, mFailedCertChain);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // mIsDelegatedCredential added in bug 1562773
  if (serVersion.EqualsASCII("2") || serVersion.EqualsASCII("3") ||
      serVersion.EqualsASCII("4") || serVersion.EqualsASCII("5")) {
    rv = aStream->ReadBoolean(&mIsDelegatedCredential);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // mNPNCompleted, mNegotiatedNPN, mResumed added in bug 1584104
  if (serVersion.EqualsASCII("4") || serVersion.EqualsASCII("5")) {
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
  if (serVersion.EqualsASCII("5")) {
    rv = aStream->ReadBoolean(&mIsBuiltCertChainRootBuiltInRoot);
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

#undef CHILD_DIAGNOSTIC_ASSERT

void TransportSecurityInfo::SerializeToIPC(IPC::Message* aMsg) {
  MutexAutoLock guard(mMutex);

  int32_t errorCode = static_cast<int32_t>(mErrorCode);

  WriteParam(aMsg, mSecurityState);
  WriteParam(aMsg, errorCode);
  WriteParam(aMsg, mServerCert);
  WriteParam(aMsg, mCipherSuite);
  WriteParam(aMsg, mProtocolVersion);
  WriteParam(aMsg, mIsDomainMismatch);
  WriteParam(aMsg, mIsNotValidAtThisTime);
  WriteParam(aMsg, mIsUntrusted);
  WriteParam(aMsg, mIsEV);
  WriteParam(aMsg, mHasIsEVStatus);
  WriteParam(aMsg, mHaveCipherSuiteAndProtocol);
  WriteParam(aMsg, mHaveCertErrorBits);
  WriteParam(aMsg, mCertificateTransparencyStatus);
  WriteParam(aMsg, mKeaGroup);
  WriteParam(aMsg, mSignatureSchemeName);
  WriteParam(aMsg, mSucceededCertChain);
  WriteParam(aMsg, mFailedCertChain);
  WriteParam(aMsg, mIsDelegatedCredential);
  WriteParam(aMsg, mNPNCompleted);
  WriteParam(aMsg, mNegotiatedNPN);
  WriteParam(aMsg, mResumed);
  WriteParam(aMsg, mIsBuiltCertChainRootBuiltInRoot);
}

bool TransportSecurityInfo::DeserializeFromIPC(const IPC::Message* aMsg,
                                               PickleIterator* aIter) {
  MutexAutoLock guard(mMutex);

  int32_t errorCode = 0;

  if (!ReadParam(aMsg, aIter, &mSecurityState) ||
      !ReadParam(aMsg, aIter, &errorCode) ||
      !ReadParam(aMsg, aIter, &mServerCert) ||
      !ReadParam(aMsg, aIter, &mCipherSuite) ||
      !ReadParam(aMsg, aIter, &mProtocolVersion) ||
      !ReadParam(aMsg, aIter, &mIsDomainMismatch) ||
      !ReadParam(aMsg, aIter, &mIsNotValidAtThisTime) ||
      !ReadParam(aMsg, aIter, &mIsUntrusted) ||
      !ReadParam(aMsg, aIter, &mIsEV) ||
      !ReadParam(aMsg, aIter, &mHasIsEVStatus) ||
      !ReadParam(aMsg, aIter, &mHaveCipherSuiteAndProtocol) ||
      !ReadParam(aMsg, aIter, &mHaveCertErrorBits) ||
      !ReadParam(aMsg, aIter, &mCertificateTransparencyStatus) ||
      !ReadParam(aMsg, aIter, &mKeaGroup) ||
      !ReadParam(aMsg, aIter, &mSignatureSchemeName) ||
      !ReadParam(aMsg, aIter, &mSucceededCertChain) ||
      !ReadParam(aMsg, aIter, &mFailedCertChain) ||
      !ReadParam(aMsg, aIter, &mIsDelegatedCredential) ||
      !ReadParam(aMsg, aIter, &mNPNCompleted) ||
      !ReadParam(aMsg, aIter, &mNegotiatedNPN) ||
      !ReadParam(aMsg, aIter, &mResumed) ||
      !ReadParam(aMsg, aIter, &mIsBuiltCertChainRootBuiltInRoot)) {
    return false;
  }

  mErrorCode = static_cast<PRErrorCode>(errorCode);
  if (mErrorCode != 0) {
    mCanceled = true;
  }

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

    CertStateBits bits;
    bits.mIsDomainMismatch = infoObject->mIsDomainMismatch;
    bits.mIsNotValidAtThisTime = infoObject->mIsNotValidAtThisTime;
    bits.mIsUntrusted = infoObject->mIsUntrusted;

    MutexAutoLock lock(mMutex);
    mErrorHosts.Put(hostPortKey, bits);
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

  CertStateBits bits;
  {
    MutexAutoLock lock(mMutex);
    if (!mErrorHosts.Get(hostPortKey, &bits)) {
      // No record was found, this host had no cert errors
      return;
    }
  }

  // This host had cert errors, update the bits correctly
  infoObject->mHaveCertErrorBits = true;
  infoObject->mIsDomainMismatch = bits.mIsDomainMismatch;
  infoObject->mIsNotValidAtThisTime = bits.mIsNotValidAtThisTime;
  infoObject->mIsUntrusted = bits.mIsUntrusted;
}

void TransportSecurityInfo::SetStatusErrorBits(nsNSSCertificate* cert,
                                               uint32_t collected_errors) {
  MutexAutoLock lock(mMutex);

  SetServerCert(cert, EVStatus::NotEV);

  mHaveCertErrorBits = true;
  mIsDomainMismatch = collected_errors & nsICertOverrideService::ERROR_MISMATCH;
  mIsNotValidAtThisTime = collected_errors & nsICertOverrideService::ERROR_TIME;
  mIsUntrusted = collected_errors & nsICertOverrideService::ERROR_UNTRUSTED;

  RememberCertErrorsTable::GetInstance().RememberCertHasError(this, SECFailure);
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

static nsresult CreateCertChain(nsTArray<RefPtr<nsIX509Cert>>& aOutput,
                                nsTArray<nsTArray<uint8_t>>&& aCertList) {
  nsTArray<nsTArray<uint8_t>> certList = std::move(aCertList);
  aOutput.Clear();
  for (auto& certBytes : certList) {
    RefPtr<nsIX509Cert> cert = nsNSSCertificate::ConstructFromDER(
        BitwiseCast<char*, uint8_t*>(certBytes.Elements()), certBytes.Length());
    if (!cert) {
      return NS_ERROR_FAILURE;
    }
    aOutput.AppendElement(cert);
  }
  return NS_OK;
}

nsresult TransportSecurityInfo::SetFailedCertChain(
    nsTArray<nsTArray<uint8_t>>&& aCertList) {
  return CreateCertChain(mFailedCertChain, std::move(aCertList));
}

NS_IMETHODIMP TransportSecurityInfo::GetServerCert(nsIX509Cert** aServerCert) {
  NS_ENSURE_ARG_POINTER(aServerCert);

  nsCOMPtr<nsIX509Cert> cert = mServerCert;
  cert.forget(aServerCert);
  return NS_OK;
}

void TransportSecurityInfo::SetServerCert(nsNSSCertificate* aServerCert,
                                          EVStatus aEVStatus) {
  MOZ_ASSERT(aServerCert);

  mServerCert = aServerCert;
  mIsEV = (aEVStatus == EVStatus::EV);
  mHasIsEVStatus = true;
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

nsresult TransportSecurityInfo::SetSucceededCertChain(
    nsTArray<nsTArray<uint8_t>>&& aCertList) {
  return CreateCertChain(mSucceededCertChain, std::move(aCertList));
}

NS_IMETHODIMP TransportSecurityInfo::SetIsBuiltCertChainRootBuiltInRoot(
    bool aIsBuiltInRoot) {
  mIsBuiltCertChainRootBuiltInRoot = aIsBuiltInRoot;
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
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aKeaGroup.Assign(mKeaGroup);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetSignatureSchemeName(nsACString& aSignatureScheme) {
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aSignatureScheme.Assign(mSignatureSchemeName);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetProtocolVersion(uint16_t* aProtocolVersion) {
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

  *aCertificateTransparencyStatus = mCertificateTransparencyStatus;
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

// static
nsTArray<nsTArray<uint8_t>> TransportSecurityInfo::CreateCertBytesArray(
    const UniqueCERTCertList& aCertChain) {
  nsTArray<nsTArray<uint8_t>> certsBytes;
  for (CERTCertListNode* n = CERT_LIST_HEAD(aCertChain);
       !CERT_LIST_END(n, aCertChain); n = CERT_LIST_NEXT(n)) {
    nsTArray<uint8_t> certBytes;
    certBytes.AppendElements(n->cert->derCert.data, n->cert->derCert.len);
    certsBytes.AppendElement(std::move(certBytes));
  }
  return certsBytes;
}

NS_IMETHODIMP
TransportSecurityInfo::GetIsDomainMismatch(bool* aIsDomainMismatch) {
  NS_ENSURE_ARG_POINTER(aIsDomainMismatch);

  *aIsDomainMismatch = mHaveCertErrorBits && mIsDomainMismatch;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetIsNotValidAtThisTime(bool* aIsNotValidAtThisTime) {
  NS_ENSURE_ARG_POINTER(aIsNotValidAtThisTime);

  *aIsNotValidAtThisTime = mHaveCertErrorBits && mIsNotValidAtThisTime;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetIsUntrusted(bool* aIsUntrusted) {
  NS_ENSURE_ARG_POINTER(aIsUntrusted);

  *aIsUntrusted = mHaveCertErrorBits && mIsUntrusted;
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
TransportSecurityInfo::GetIsDelegatedCredential(bool* aIsDelegCred) {
  NS_ENSURE_ARG_POINTER(aIsDelegCred);
  if (!mHaveCipherSuiteAndProtocol) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aIsDelegCred = mIsDelegatedCredential;
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
  *aResumed = mResumed;
  return NS_OK;
}

void TransportSecurityInfo::SetResumed(bool aResumed) {
  MutexAutoLock lock(mMutex);
  mResumed = aResumed;
}

}  // namespace psm
}  // namespace mozilla

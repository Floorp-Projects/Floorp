/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransportSecurityInfo.h"

#include "DateTimeFormat.h"
#include "PSMRunnable.h"
#include "mozilla/Casting.h"
#include "nsComponentManagerUtils.h"
#include "nsIArray.h"
#include "nsICertOverrideService.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIWebProgressListener.h"
#include "nsIX509CertValidity.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"
#include "mozpkix/pkixtypes.h"
#include "secerr.h"

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
      mIsDomainMismatch(false),
      mIsNotValidAtThisTime(false),
      mIsUntrusted(false),
      mIsEV(false),
      mHasIsEVStatus(false),
      mHaveCipherSuiteAndProtocol(false),
      mHaveCertErrorBits(false),
      mCanceled(false),
      mMutex("TransportSecurityInfo::mMutex"),
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
  rv = aStream->WriteWStringZ(NS_ConvertUTF8toUTF16("1").get());
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

  rv = NS_WriteOptionalCompoundObject(aStream, mSucceededCertChain,
                                      NS_GET_IID(nsIX509CertList), true);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // END moved from nsISSLStatus

  rv = NS_WriteOptionalCompoundObject(aStream, mFailedCertChain,
                                      NS_GET_IID(nsIX509CertList), true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

#define CHILD_DIAGNOSTIC_ASSERT(condition, message)       \
  if (XRE_GetProcessType() == GeckoProcessType_Content) { \
    MOZ_DIAGNOSTIC_ASSERT(condition, message);            \
  }

// This is for backward compatability to be able to read nsISSLStatus
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
    nsCOMPtr<nsISupports> succeededCertChainSupports;
    rv = NS_ReadOptionalObject(aStream, true,
                               getter_AddRefs(succeededCertChainSupports));
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
    mSucceededCertChain = do_QueryInterface(succeededCertChainSupports);

    // Read only to consume bytes from the stream.
    nsCOMPtr<nsISupports> failedCertChainSupports;
    rv = NS_ReadOptionalObject(aStream, true,
                               getter_AddRefs(failedCertChainSupports));
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return rv;
}

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
  if (!serVersion.EqualsASCII("1")) {
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

    nsCOMPtr<nsISupports> succeededCertChainSupports;
    rv = NS_ReadOptionalObject(aStream, true,
                               getter_AddRefs(succeededCertChainSupports));
    CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserialization should not fail");
    if (NS_FAILED(rv)) {
      return rv;
    }
    mSucceededCertChain = do_QueryInterface(succeededCertChainSupports);
  }
  // END moved from nsISSLStatus

  nsCOMPtr<nsISupports> failedCertChainSupports;
  rv = NS_ReadOptionalObject(aStream, true,
                             getter_AddRefs(failedCertChainSupports));
  CHILD_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv), "Deserialization should not fail");
  if (NS_FAILED(rv)) {
    return rv;
  }
  mFailedCertChain = do_QueryInterface(failedCertChainSupports);

  return NS_OK;
}

#undef CHILD_DIAGNOSTIC_ASSERT

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
TransportSecurityInfo::GetFailedCertChain(nsIX509CertList** _result) {
  MOZ_ASSERT(_result);

  *_result = mFailedCertChain;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

nsresult TransportSecurityInfo::SetFailedCertChain(
    UniqueCERTCertList certList) {
  // nsNSSCertList takes ownership of certList
  mFailedCertChain = new nsNSSCertList(std::move(certList));

  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetServerCert(nsIX509Cert** aServerCert) {
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
TransportSecurityInfo::GetSucceededCertChain(nsIX509CertList** _result) {
  NS_ENSURE_ARG_POINTER(_result);

  nsCOMPtr<nsIX509CertList> tmpList = mSucceededCertChain;
  tmpList.forget(_result);

  return NS_OK;
}

nsresult TransportSecurityInfo::SetSucceededCertChain(
    UniqueCERTCertList aCertList) {
  // nsNSSCertList takes ownership of certList
  mSucceededCertChain = new nsNSSCertList(std::move(aCertList));

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

void TransportSecurityInfo::SetCertificateTransparencyInfo(
    const mozilla::psm::CertificateTransparencyInfo& info) {
  using mozilla::ct::CTPolicyCompliance;

  mCertificateTransparencyStatus =
      nsITransportSecurityInfo::CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE;

  if (!info.enabled) {
    // CT disabled.
    return;
  }

  switch (info.policyCompliance) {
    case CTPolicyCompliance::Compliant:
      mCertificateTransparencyStatus =
          nsITransportSecurityInfo::CERTIFICATE_TRANSPARENCY_POLICY_COMPLIANT;
      break;
    case CTPolicyCompliance::NotEnoughScts:
      mCertificateTransparencyStatus = nsITransportSecurityInfo ::
          CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS;
      break;
    case CTPolicyCompliance::NotDiverseScts:
      mCertificateTransparencyStatus = nsITransportSecurityInfo ::
          CERTIFICATE_TRANSPARENCY_POLICY_NOT_DIVERSE_SCTS;
      break;
    case CTPolicyCompliance::Unknown:
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected CTPolicyCompliance type");
  }
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

}  // namespace psm
}  // namespace mozilla

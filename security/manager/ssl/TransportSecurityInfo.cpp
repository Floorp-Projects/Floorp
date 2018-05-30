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
#include "nsReadableUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"
#include "pkix/pkixtypes.h"
#include "secerr.h"

//#define DEBUG_SSL_VERBOSE //Enable this define to get minimal
                            //reports when doing SSL read/write

//#define DUMP_BUFFER  //Enable this define along with
                       //DEBUG_SSL_VERBOSE to dump SSL
                       //read/write buffer to a log.
                       //Uses PR_LOG except on Mac where
                       //we always write out to our own
                       //file.

namespace mozilla { namespace psm {

TransportSecurityInfo::TransportSecurityInfo()
  : mMutex("TransportSecurityInfo::mMutex")
  , mSecurityState(nsIWebProgressListener::STATE_IS_INSECURE)
  , mSubRequestsBrokenSecurity(0)
  , mSubRequestsNoSecurity(0)
  , mErrorCode(0)
  , mPort(0)
{
}

NS_IMPL_ISUPPORTS(TransportSecurityInfo,
                  nsITransportSecurityInfo,
                  nsIInterfaceRequestor,
                  nsISSLStatusProvider,
                  nsIAssociatedContentSecurity,
                  nsISerializable,
                  nsIClassInfo)

void
TransportSecurityInfo::SetHostName(const char* host)
{
  mHostName.Assign(host);
}

void
TransportSecurityInfo::SetPort(int32_t aPort)
{
  mPort = aPort;
}

void
TransportSecurityInfo::SetOriginAttributes(
  const OriginAttributes& aOriginAttributes)
{
  mOriginAttributes = aOriginAttributes;
}

NS_IMETHODIMP
TransportSecurityInfo::GetErrorCode(int32_t* state)
{
  MutexAutoLock lock(mMutex);

  *state = mErrorCode;
  return NS_OK;
}

void
TransportSecurityInfo::SetCanceled(PRErrorCode errorCode)
{
  MutexAutoLock lock(mMutex);

  mErrorCode = errorCode;
}

NS_IMETHODIMP
TransportSecurityInfo::GetSecurityState(uint32_t* state)
{
  *state = mSecurityState;
  return NS_OK;
}

void
TransportSecurityInfo::SetSecurityState(uint32_t aState)
{
  mSecurityState = aState;
}

NS_IMETHODIMP
TransportSecurityInfo::GetCountSubRequestsBrokenSecurity(
  int32_t *aSubRequestsBrokenSecurity)
{
  *aSubRequestsBrokenSecurity = mSubRequestsBrokenSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::SetCountSubRequestsBrokenSecurity(
  int32_t aSubRequestsBrokenSecurity)
{
  mSubRequestsBrokenSecurity = aSubRequestsBrokenSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetCountSubRequestsNoSecurity(
  int32_t *aSubRequestsNoSecurity)
{
  *aSubRequestsNoSecurity = mSubRequestsNoSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::SetCountSubRequestsNoSecurity(
  int32_t aSubRequestsNoSecurity)
{
  mSubRequestsNoSecurity = aSubRequestsNoSecurity;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetErrorCodeString(nsAString& aErrorString)
{
  MutexAutoLock lock(mMutex);

  const char* codeName = PR_ErrorToName(mErrorCode);
  aErrorString.Truncate();
  if (codeName) {
    aErrorString = NS_ConvertASCIItoUTF16(codeName);
  }

  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetInterface(const nsIID & uuid, void * *result)
{
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
#define TRANSPORTSECURITYINFOMAGIC { 0xa9863a23, 0x1faa, 0x4169, \
  { 0xb0, 0xd2, 0x81, 0x29, 0xec, 0x7c, 0xb1, 0xde } }
static NS_DEFINE_CID(kTransportSecurityInfoMagic, TRANSPORTSECURITYINFOMAGIC);

NS_IMETHODIMP
TransportSecurityInfo::Write(nsIObjectOutputStream* stream)
{
  nsresult rv = stream->WriteID(kTransportSecurityInfoMagic);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MutexAutoLock lock(mMutex);

  rv = stream->Write32(mSecurityState);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = stream->Write32(mSubRequestsBrokenSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = stream->Write32(mSubRequestsNoSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = stream->Write32(static_cast<uint32_t>(mErrorCode));
  if (NS_FAILED(rv)) {
    return rv;
  }
  // Write empty string where mErrorMessageCached lived before.
  rv = stream->WriteWStringZ(NS_ConvertUTF8toUTF16("").get());
  if (NS_FAILED(rv)) {
    return rv;
  }

  // For successful connections and for connections with overridable errors,
  // mSSLStatus will be non-null. However, for connections with non-overridable
  // errors, it will be null.
  nsCOMPtr<nsISerializable> serializable(mSSLStatus);
  rv = NS_WriteOptionalCompoundObject(stream,
                                      serializable,
                                      NS_GET_IID(nsISSLStatus),
                                      true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = NS_WriteOptionalCompoundObject(stream,
                                      mFailedCertChain,
                                      NS_GET_IID(nsIX509CertList),
                                      true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::Read(nsIObjectInputStream* stream)
{
  nsID id;
  nsresult rv = stream->ReadID(&id);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!id.Equals(kTransportSecurityInfoMagic)) {
    return NS_ERROR_UNEXPECTED;
  }

  MutexAutoLock lock(mMutex);

  rv = stream->Read32(&mSecurityState);
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint32_t subRequestsBrokenSecurity;
  rv = stream->Read32(&subRequestsBrokenSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (subRequestsBrokenSecurity >
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return NS_ERROR_UNEXPECTED;
  }
  mSubRequestsBrokenSecurity = subRequestsBrokenSecurity;
  uint32_t subRequestsNoSecurity;
  rv = stream->Read32(&subRequestsNoSecurity);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (subRequestsNoSecurity >
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return NS_ERROR_UNEXPECTED;
  }
  mSubRequestsNoSecurity = subRequestsNoSecurity;
  uint32_t errorCode;
  rv = stream->Read32(&errorCode);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // PRErrorCode will be a negative value
  mErrorCode = static_cast<PRErrorCode>(errorCode);

  // We don't do error messages here anymore.
  nsString unused;
  rv = stream->ReadString(unused);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // For successful connections and for connections with overridable errors,
  // mSSLStatus will be non-null. For connections with non-overridable errors,
  // it will be null.
  nsCOMPtr<nsISupports> supports;
  rv = NS_ReadOptionalObject(stream, true, getter_AddRefs(supports));
  if (NS_FAILED(rv)) {
    return rv;
  }
  mSSLStatus = BitwiseCast<nsSSLStatus*, nsISupports*>(supports.get());

  nsCOMPtr<nsISupports> failedCertChainSupports;
  rv = NS_ReadOptionalObject(stream, true, getter_AddRefs(failedCertChainSupports));
  if (NS_FAILED(rv)) {
    return rv;
  }
  mFailedCertChain = do_QueryInterface(failedCertChainSupports);

  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetInterfaces(uint32_t *count, nsIID * **array)
{
  *count = 0;
  *array = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetScriptableHelper(nsIXPCScriptable **_retval)
{
  *_retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetContractID(nsACString& aContractID)
{
  aContractID.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetClassDescription(nsACString& aClassDescription)
{
  aClassDescription.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
TransportSecurityInfo::GetClassID(nsCID * *aClassID)
{
  *aClassID = (nsCID*) moz_xmalloc(sizeof(nsCID));
  if (!*aClassID)
    return NS_ERROR_OUT_OF_MEMORY;
  return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP
TransportSecurityInfo::GetFlags(uint32_t *aFlags)
{
  *aFlags = 0;
  return NS_OK;
}

static NS_DEFINE_CID(kNSSSocketInfoCID, TRANSPORTSECURITYINFO_CID);

NS_IMETHODIMP
TransportSecurityInfo::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  *aClassIDNoAlloc = kNSSSocketInfoCID;
  return NS_OK;
}

nsresult
TransportSecurityInfo::GetSSLStatus(nsISSLStatus** _result)
{
  NS_ENSURE_ARG_POINTER(_result);

  *_result = mSSLStatus;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

void
TransportSecurityInfo::SetSSLStatus(nsSSLStatus *aSSLStatus)
{
  mSSLStatus = aSSLStatus;
}

// RememberCertErrorsTable

/*static*/ RememberCertErrorsTable*
RememberCertErrorsTable::sInstance = nullptr;

RememberCertErrorsTable::RememberCertErrorsTable()
  : mErrorHosts()
  , mMutex("RememberCertErrorsTable::mMutex")
{
}

static nsresult
GetHostPortKey(TransportSecurityInfo* infoObject, /*out*/ nsCString& result)
{
  MOZ_ASSERT(infoObject);
  NS_ENSURE_ARG(infoObject);

  result.Truncate();

  result.Assign(infoObject->GetHostName());
  result.Append(':');
  result.AppendInt(infoObject->GetPort());

  return NS_OK;
}

void
RememberCertErrorsTable::RememberCertHasError(TransportSecurityInfo* infoObject,
                                              nsSSLStatus* status,
                                              SECStatus certVerificationResult)
{
  nsresult rv;

  nsAutoCString hostPortKey;
  rv = GetHostPortKey(infoObject, hostPortKey);
  if (NS_FAILED(rv))
    return;

  if (certVerificationResult != SECSuccess) {
    MOZ_ASSERT(status, "Must have nsSSLStatus object when remembering flags");

    if (!status)
      return;

    CertStateBits bits;
    bits.mIsDomainMismatch = status->mIsDomainMismatch;
    bits.mIsNotValidAtThisTime = status->mIsNotValidAtThisTime;
    bits.mIsUntrusted = status->mIsUntrusted;

    MutexAutoLock lock(mMutex);
    mErrorHosts.Put(hostPortKey, bits);
  }
  else {
    MutexAutoLock lock(mMutex);
    mErrorHosts.Remove(hostPortKey);
  }
}

void
RememberCertErrorsTable::LookupCertErrorBits(TransportSecurityInfo* infoObject,
                                             nsSSLStatus* status)
{
  // Get remembered error bits from our cache, because of SSL session caching
  // the NSS library potentially hasn't notified us for this socket.
  if (status->mHaveCertErrorBits)
    // Rather do not modify bits if already set earlier
    return;

  nsresult rv;

  nsAutoCString hostPortKey;
  rv = GetHostPortKey(infoObject, hostPortKey);
  if (NS_FAILED(rv))
    return;

  CertStateBits bits;
  {
    MutexAutoLock lock(mMutex);
    if (!mErrorHosts.Get(hostPortKey, &bits))
      // No record was found, this host had no cert errors
      return;
  }

  // This host had cert errors, update the bits correctly
  status->mHaveCertErrorBits = true;
  status->mIsDomainMismatch = bits.mIsDomainMismatch;
  status->mIsNotValidAtThisTime = bits.mIsNotValidAtThisTime;
  status->mIsUntrusted = bits.mIsUntrusted;
}

void
TransportSecurityInfo::SetStatusErrorBits(nsNSSCertificate* cert,
                                          uint32_t collected_errors)
{
  MutexAutoLock lock(mMutex);

  if (!mSSLStatus) {
    mSSLStatus = new nsSSLStatus();
  }

  mSSLStatus->SetServerCert(cert, EVStatus::NotEV);
  mSSLStatus->SetFailedCertChain(mFailedCertChain);

  mSSLStatus->mHaveCertErrorBits = true;
  mSSLStatus->mIsDomainMismatch =
    collected_errors & nsICertOverrideService::ERROR_MISMATCH;
  mSSLStatus->mIsNotValidAtThisTime =
    collected_errors & nsICertOverrideService::ERROR_TIME;
  mSSLStatus->mIsUntrusted =
    collected_errors & nsICertOverrideService::ERROR_UNTRUSTED;

  RememberCertErrorsTable::GetInstance().RememberCertHasError(this,
                                                              mSSLStatus,
                                                              SECFailure);
}

NS_IMETHODIMP
TransportSecurityInfo::GetFailedCertChain(nsIX509CertList** _result)
{
  MOZ_ASSERT(_result);

  *_result = mFailedCertChain;
  NS_IF_ADDREF(*_result);

  return NS_OK;
}

nsresult
TransportSecurityInfo::SetFailedCertChain(UniqueCERTCertList certList)
{
  // nsNSSCertList takes ownership of certList
  mFailedCertChain = new nsNSSCertList(std::move(certList));

  return NS_OK;
}

} } // namespace mozilla::psm

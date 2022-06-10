/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSIOLayer.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "NSSCertDBTrustDomain.h"
#include "NSSErrorsService.h"
#include "PSMRunnable.h"
#include "SSLServerCertVerification.h"
#include "ScopedNSSTypes.h"
#include "SharedSSLState.h"
#include "cert_storage/src/cert_storage.h"
#include "keyhi.h"
#include "mozilla/Base64.h"
#include "mozilla/Casting.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/RandomNum.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/net/SSLTokensCache.h"
#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/psm/IPCClientCertsChild.h"
#include "mozilla/psm/PIPCClientCertsChild.h"
#include "mozpkix/pkixnss.h"
#include "mozpkix/pkixtypes.h"
#include "mozpkix/pkixutil.h"
#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsCRT.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsClientAuthRemember.h"
#include "nsContentUtils.h"
#include "nsIClientAuthDialogs.h"
#include "nsISocketProvider.h"
#include "nsISocketTransport.h"
#include "nsIWebProgressListener.h"
#include "nsNSSCertHelper.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "prmem.h"
#include "prnetdb.h"
#include "secder.h"
#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslexp.h"
#include "sslproto.h"

using namespace mozilla::psm;
using namespace mozilla::ipc;

//#define DEBUG_SSL_VERBOSE //Enable this define to get minimal
// reports when doing SSL read/write

//#define DUMP_BUFFER  //Enable this define along with
// DEBUG_SSL_VERBOSE to dump SSL
// read/write buffer to a log.
// Uses PR_LOG except on Mac where
// we always write out to our own
// file.

namespace {

// The NSSSocketInfo tls flags are meant to be opaque to most calling
// applications but provide a mechanism for direct TLS manipulation when
// experimenting with new features in the scope of a single socket. They do not
// create a persistent ABI.
//
// Use of these flags creates a new 'sharedSSLState' so existing states for
// intolerance are not carried to sockets that use these flags (and intolerance
// they discover does not impact other normal sockets not using the flags.)
//
// Their current definitions are:
//
// bits 0-2 (mask 0x07) specify the max tls version
//          0 means no override 1->4 are 1.0, 1.1, 1.2, 1.3, 4->7 unused
// bits 3-5 (mask 0x38) specify the tls fallback limit
//          0 means no override, values 1->4 match prefs
// bit    6 (mask 0x40) was used to specify compat mode. Temporarily reserved.

enum {
  kTLSProviderFlagMaxVersion10 = 0x01,
  kTLSProviderFlagMaxVersion11 = 0x02,
  kTLSProviderFlagMaxVersion12 = 0x03,
  kTLSProviderFlagMaxVersion13 = 0x04,
};

static uint32_t getTLSProviderFlagMaxVersion(uint32_t flags) {
  return (flags & 0x07);
}

static uint32_t getTLSProviderFlagFallbackLimit(uint32_t flags) {
  return (flags & 0x38) >> 3;
}

#define MAX_ALPN_LENGTH 255

void getSiteKey(const nsACString& hostName, uint16_t port,
                /*out*/ nsACString& key) {
  key = hostName;
  key.AppendLiteral(":");
  key.AppendInt(port);
}

}  // unnamed namespace

extern LazyLogModule gPIPNSSLog;

nsNSSSocketInfo::nsNSSSocketInfo(SharedSSLState& aState, uint32_t providerFlags,
                                 uint32_t providerTlsFlags)
    : CommonSocketControl(providerFlags),
      mFd(nullptr),
      mCertVerificationState(before_cert_verification),
      mSharedState(aState),
      mForSTARTTLS(false),
      mHandshakePending(true),
      mPreliminaryHandshakeDone(false),
      mEarlyDataAccepted(false),
      mDenyClientCert(false),
      mFalseStartCallbackCalled(false),
      mFalseStarted(false),
      mIsFullHandshake(false),
      mNotedTimeUntilReady(false),
      mEchExtensionStatus(EchExtensionStatus::kNotPresent),
      mIsShortWritePending(false),
      mShortWritePendingByte(0),
      mShortWriteOriginalAmount(-1),
      mKEAUsed(nsISSLSocketControl::KEY_EXCHANGE_UNKNOWN),
      mKEAKeyBits(0),
      mMACAlgorithmUsed(nsISSLSocketControl::SSL_MAC_UNKNOWN),
      mProviderTlsFlags(providerTlsFlags),
      mSocketCreationTimestamp(TimeStamp::Now()),
      mPlaintextBytesRead(0) {
  mTLSVersionRange.min = 0;
  mTLSVersionRange.max = 0;
}

nsNSSSocketInfo::~nsNSSSocketInfo() = default;

NS_IMPL_ISUPPORTS_INHERITED(nsNSSSocketInfo, TransportSecurityInfo,
                            nsISSLSocketControl)

NS_IMETHODIMP
nsNSSSocketInfo::GetProviderTlsFlags(uint32_t* aProviderTlsFlags) {
  *aProviderTlsFlags = mProviderTlsFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetKEAUsed(int16_t* aKea) {
  *aKea = mKEAUsed;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetKEAKeyBits(uint32_t* aKeyBits) {
  *aKeyBits = mKEAKeyBits;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetSSLVersionOffered(int16_t* aSSLVersionOffered) {
  *aSSLVersionOffered = mTLSVersionRange.max;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetMACAlgorithmUsed(int16_t* aMac) {
  *aMac = mMACAlgorithmUsed;
  return NS_OK;
}

void nsNSSSocketInfo::NoteTimeUntilReady() {
  MutexAutoLock lock(mMutex);
  if (mNotedTimeUntilReady) return;

  mNotedTimeUntilReady = true;

  Telemetry::HistogramID time_histogram;
  switch (GetEchExtensionStatus()) {
    case EchExtensionStatus::kNotPresent:
      time_histogram = Telemetry::SSL_TIME_UNTIL_READY;
      break;
    case EchExtensionStatus::kGREASE:
      time_histogram = Telemetry::SSL_TIME_UNTIL_READY_ECH_GREASE;
      break;
    case EchExtensionStatus::kReal:
      time_histogram = Telemetry::SSL_TIME_UNTIL_READY_ECH;
      break;
  }
  // This will include TCP and proxy tunnel wait time
  Telemetry::AccumulateTimeDelta(time_histogram, mSocketCreationTimestamp,
                                 TimeStamp::Now());

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] nsNSSSocketInfo::NoteTimeUntilReady\n", mFd));
}

void nsNSSSocketInfo::SetHandshakeCompleted() {
  if (!mHandshakeCompleted) {
    enum HandshakeType {
      Resumption = 1,
      FalseStarted = 2,
      ChoseNotToFalseStart = 3,
      NotAllowedToFalseStart = 4,
    };

    HandshakeType handshakeType = !IsFullHandshake() ? Resumption
                                  : mFalseStarted    ? FalseStarted
                                  : mFalseStartCallbackCalled
                                      ? ChoseNotToFalseStart
                                      : NotAllowedToFalseStart;
    MutexAutoLock lock(mMutex);
    // This will include TCP and proxy tunnel wait time
    Telemetry::AccumulateTimeDelta(
        Telemetry::SSL_TIME_UNTIL_HANDSHAKE_FINISHED_KEYED_BY_KA, mKeaGroup,
        mSocketCreationTimestamp, TimeStamp::Now());

    // If the handshake is completed for the first time from just 1 callback
    // that means that TLS session resumption must have been used.
    Telemetry::Accumulate(Telemetry::SSL_RESUMED_SESSION,
                          handshakeType == Resumption);
    Telemetry::Accumulate(Telemetry::SSL_HANDSHAKE_TYPE, handshakeType);
  }

  // Remove the plaintext layer as it is not needed anymore.
  // The plaintext layer is not always present - so it's not a fatal error if it
  // cannot be removed.
  // Note that PR_PopIOLayer may modify its stack, so a pointer returned by
  // PR_GetIdentitiesLayer may not point to what we think it points to after
  // calling PR_PopIOLayer. We must operate on the pointer returned by
  // PR_PopIOLayer.
  if (PR_GetIdentitiesLayer(mFd,
                            nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity)) {
    PRFileDesc* poppedPlaintext =
        PR_PopIOLayer(mFd, nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity);
    poppedPlaintext->dtor(poppedPlaintext);
  }

  mHandshakeCompleted = true;

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] nsNSSSocketInfo::SetHandshakeCompleted\n", (void*)mFd));

  mIsFullHandshake = false;  // reset for next handshake on this connection

  if (mTlsHandshakeCallback) {
    auto callback = std::move(mTlsHandshakeCallback);
    Unused << callback->HandshakeDone();
  }
}

void nsNSSSocketInfo::SetNegotiatedNPN(const char* value, uint32_t length) {
  MutexAutoLock lock(mMutex);
  if (!value) {
    mNegotiatedNPN.Truncate();
  } else {
    mNegotiatedNPN.Assign(value, length);
  }
  mNPNCompleted = true;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetAlpnEarlySelection(nsACString& aAlpnSelected) {
  aAlpnSelected.Truncate();

  SSLPreliminaryChannelInfo info;
  SECStatus rv = SSL_GetPreliminaryChannelInfo(mFd, &info, sizeof(info));
  if (rv != SECSuccess || !info.canSendEarlyData) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  SSLNextProtoState alpnState;
  unsigned char chosenAlpn[MAX_ALPN_LENGTH];
  unsigned int chosenAlpnLen;
  rv = SSL_GetNextProto(mFd, &alpnState, chosenAlpn, &chosenAlpnLen,
                        AssertedCast<unsigned int>(ArrayLength(chosenAlpn)));

  if (rv != SECSuccess) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (alpnState == SSL_NEXT_PROTO_EARLY_VALUE) {
    aAlpnSelected.Assign(BitwiseCast<char*, unsigned char*>(chosenAlpn),
                         chosenAlpnLen);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetEarlyDataAccepted(bool* aAccepted) {
  *aAccepted = mEarlyDataAccepted;
  return NS_OK;
}

void nsNSSSocketInfo::SetEarlyDataAccepted(bool aAccepted) {
  mEarlyDataAccepted = aAccepted;
}

bool nsNSSSocketInfo::GetDenyClientCert() { return mDenyClientCert; }

void nsNSSSocketInfo::SetDenyClientCert(bool aDenyClientCert) {
  mDenyClientCert = aDenyClientCert;
}

NS_IMETHODIMP
nsNSSSocketInfo::DriveHandshake() {
  if (!mFd) {
    return NS_ERROR_FAILURE;
  }
  if (IsCanceled()) {
    PRErrorCode errorCode = GetErrorCode();
    MOZ_DIAGNOSTIC_ASSERT(errorCode, "handshake cancelled without error code");
    return GetXPCOMFromNSSError(errorCode);
  }

  SECStatus rv = SSL_ForceHandshake(mFd);

  if (rv != SECSuccess) {
    PRErrorCode errorCode = PR_GetError();
    MOZ_ASSERT(errorCode, "handshake failed without error code");
    // There is a bug in NSS. Sometimes SSL_ForceHandshake will return
    // SECFailure without setting an error code. In these cases, cancel
    // the connection with SEC_ERROR_LIBRARY_FAILURE.
    if (!errorCode) {
      errorCode = SEC_ERROR_LIBRARY_FAILURE;
    }
    if (errorCode == PR_WOULD_BLOCK_ERROR) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    SetCanceled(errorCode);
    return GetXPCOMFromNSSError(errorCode);
  }
  return NS_OK;
}

bool nsNSSSocketInfo::GetForSTARTTLS() { return mForSTARTTLS; }

void nsNSSSocketInfo::SetForSTARTTLS(bool aForSTARTTLS) {
  mForSTARTTLS = aForSTARTTLS;
}

NS_IMETHODIMP
nsNSSSocketInfo::ProxyStartSSL() { return ActivateSSL(); }

NS_IMETHODIMP
nsNSSSocketInfo::StartTLS() { return ActivateSSL(); }

NS_IMETHODIMP
nsNSSSocketInfo::SetNPNList(nsTArray<nsCString>& protocolArray) {
  if (!mFd) return NS_ERROR_FAILURE;

  // the npn list is a concatenated list of 8 bit byte strings.
  nsCString npnList;

  for (uint32_t index = 0; index < protocolArray.Length(); ++index) {
    if (protocolArray[index].IsEmpty() || protocolArray[index].Length() > 255)
      return NS_ERROR_ILLEGAL_VALUE;

    npnList.Append(protocolArray[index].Length());
    npnList.Append(protocolArray[index]);
  }

  if (SSL_SetNextProtoNego(
          mFd, BitwiseCast<const unsigned char*, const char*>(npnList.get()),
          npnList.Length()) != SECSuccess)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult nsNSSSocketInfo::ActivateSSL() {
  if (SECSuccess != SSL_OptionSet(mFd, SSL_SECURITY, true))
    return NS_ERROR_FAILURE;
  if (SECSuccess != SSL_ResetHandshake(mFd, false)) return NS_ERROR_FAILURE;

  mHandshakePending = true;

  return SetResumptionTokenFromExternalCache();
}

nsresult nsNSSSocketInfo::GetFileDescPtr(PRFileDesc** aFilePtr) {
  *aFilePtr = mFd;
  return NS_OK;
}

nsresult nsNSSSocketInfo::SetFileDescPtr(PRFileDesc* aFilePtr) {
  mFd = aFilePtr;
  return NS_OK;
}

void nsNSSSocketInfo::SetCertVerificationWaiting() {
  // mCertVerificationState may be before_cert_verification for the first
  // handshake on the connection, or after_cert_verification for subsequent
  // renegotiation handshakes.
  MOZ_ASSERT(mCertVerificationState != waiting_for_cert_verification,
             "Invalid state transition to waiting_for_cert_verification");
  mCertVerificationState = waiting_for_cert_verification;
}

// Be careful that SetCertVerificationResult does NOT get called while we are
// processing a SSL callback function, because SSL_AuthCertificateComplete will
// attempt to acquire locks that are already held by libssl when it calls
// callbacks.
void nsNSSSocketInfo::SetCertVerificationResult(PRErrorCode errorCode) {
  MOZ_ASSERT(mCertVerificationState == waiting_for_cert_verification,
             "Invalid state transition to cert_verification_finished");

  if (mFd) {
    SECStatus rv = SSL_AuthCertificateComplete(mFd, errorCode);
    // Only replace errorCode if there was originally no error
    if (rv != SECSuccess && errorCode == 0) {
      errorCode = PR_GetError();
      if (errorCode == 0) {
        NS_ERROR("SSL_AuthCertificateComplete didn't set error code");
        errorCode = PR_INVALID_STATE_ERROR;
      }
    }
  }

  if (errorCode) {
    mFailedVerification = true;
    SetCanceled(errorCode);
  }

  if (mPlaintextBytesRead && !errorCode) {
    Telemetry::Accumulate(Telemetry::SSL_BYTES_BEFORE_CERT_CALLBACK,
                          AssertedCast<uint32_t>(mPlaintextBytesRead));
  }

  mCertVerificationState = after_cert_verification;
}

SharedSSLState& nsNSSSocketInfo::SharedState() { return mSharedState; }

void nsNSSSocketInfo::SetSharedOwningReference(SharedSSLState* aRef) {
  mOwningSharedRef = aRef;
}

NS_IMETHODIMP
nsNSSSocketInfo::DisableEarlyData() {
  if (!mFd) {
    return NS_OK;
  }
  if (IsCanceled()) {
    return NS_OK;
  }

  if (SSL_OptionSet(mFd, SSL_ENABLE_0RTT_DATA, false) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetHandshakeCallbackListener(
    nsITlsHandshakeCallbackListener* callback) {
  mTlsHandshakeCallback = callback;
  return NS_OK;
}

void nsSSLIOLayerHelpers::Cleanup() {
  MutexAutoLock lock(mutex);
  mTLSIntoleranceInfo.Clear();
  mInsecureFallbackSites.Clear();
}

namespace {

enum Operation { reading, writing, not_reading_or_writing };

int32_t checkHandshake(int32_t bytesTransfered, bool wasReading,
                       PRFileDesc* ssl_layer_fd, nsNSSSocketInfo* socketInfo);

nsNSSSocketInfo* getSocketInfoIfRunning(PRFileDesc* fd, Operation op) {
  if (!fd || !fd->lower || !fd->secret ||
      fd->identity != nsSSLIOLayerHelpers::nsSSLIOLayerIdentity) {
    NS_ERROR("bad file descriptor passed to getSocketInfoIfRunning");
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return nullptr;
  }

  nsNSSSocketInfo* socketInfo = (nsNSSSocketInfo*)fd->secret;

  if (socketInfo->IsCanceled()) {
    PRErrorCode err = socketInfo->GetErrorCode();
    PR_SetError(err, 0);
    if (op == reading || op == writing) {
      // We must do TLS intolerance checks for reads and writes, for timeouts
      // in particular.
      (void)checkHandshake(-1, op == reading, fd, socketInfo);
    }

    // If we get here, it is probably because cert verification failed and this
    // is the first I/O attempt since that failure.
    return nullptr;
  }

  return socketInfo;
}

}  // namespace

static PRStatus nsSSLIOLayerConnect(PRFileDesc* fd, const PRNetAddr* addr,
                                    PRIntervalTime timeout) {
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] connecting SSL socket\n", (void*)fd));
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing)) return PR_FAILURE;

  PRStatus status = fd->lower->methods->connect(fd->lower, addr, timeout);
  if (status != PR_SUCCESS) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
            ("[%p] Lower layer connect error: %d\n", (void*)fd, PR_GetError()));
    return status;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] Connect\n", (void*)fd));
  return status;
}

void nsSSLIOLayerHelpers::rememberTolerantAtVersion(const nsACString& hostName,
                                                    int16_t port,
                                                    uint16_t tolerant) {
  nsCString key;
  getSiteKey(hostName, port, key);

  MutexAutoLock lock(mutex);

  IntoleranceEntry entry;
  if (mTLSIntoleranceInfo.Get(key, &entry)) {
    entry.AssertInvariant();
    entry.tolerant = std::max(entry.tolerant, tolerant);
    if (entry.intolerant != 0 && entry.intolerant <= entry.tolerant) {
      entry.intolerant = entry.tolerant + 1;
      entry.intoleranceReason = 0;  // lose the reason
    }
  } else {
    entry.tolerant = tolerant;
    entry.intolerant = 0;
    entry.intoleranceReason = 0;
  }

  entry.AssertInvariant();

  mTLSIntoleranceInfo.InsertOrUpdate(key, entry);
}

void nsSSLIOLayerHelpers::forgetIntolerance(const nsACString& hostName,
                                            int16_t port) {
  nsCString key;
  getSiteKey(hostName, port, key);

  MutexAutoLock lock(mutex);

  IntoleranceEntry entry;
  if (mTLSIntoleranceInfo.Get(key, &entry)) {
    entry.AssertInvariant();

    entry.intolerant = 0;
    entry.intoleranceReason = 0;

    entry.AssertInvariant();
    mTLSIntoleranceInfo.InsertOrUpdate(key, entry);
  }
}

bool nsSSLIOLayerHelpers::fallbackLimitReached(const nsACString& hostName,
                                               uint16_t intolerant) {
  if (isInsecureFallbackSite(hostName)) {
    return intolerant <= SSL_LIBRARY_VERSION_TLS_1_0;
  }
  return intolerant <= mVersionFallbackLimit;
}

// returns true if we should retry the handshake
bool nsSSLIOLayerHelpers::rememberIntolerantAtVersion(
    const nsACString& hostName, int16_t port, uint16_t minVersion,
    uint16_t intolerant, PRErrorCode intoleranceReason) {
  if (intolerant <= minVersion || fallbackLimitReached(hostName, intolerant)) {
    // We can't fall back any further. Assume that intolerance isn't the issue.
    forgetIntolerance(hostName, port);
    return false;
  }

  nsCString key;
  getSiteKey(hostName, port, key);

  MutexAutoLock lock(mutex);

  IntoleranceEntry entry;
  if (mTLSIntoleranceInfo.Get(key, &entry)) {
    entry.AssertInvariant();
    if (intolerant <= entry.tolerant) {
      // We already know the server is tolerant at an equal or higher version.
      return false;
    }
    if ((entry.intolerant != 0 && intolerant >= entry.intolerant)) {
      // We already know that the server is intolerant at a lower version.
      return true;
    }
  } else {
    entry.tolerant = 0;
  }

  entry.intolerant = intolerant;
  entry.intoleranceReason = intoleranceReason;
  entry.AssertInvariant();
  mTLSIntoleranceInfo.InsertOrUpdate(key, entry);

  return true;
}

void nsSSLIOLayerHelpers::adjustForTLSIntolerance(
    const nsACString& hostName, int16_t port,
    /*in/out*/ SSLVersionRange& range) {
  IntoleranceEntry entry;

  {
    nsCString key;
    getSiteKey(hostName, port, key);

    MutexAutoLock lock(mutex);
    if (!mTLSIntoleranceInfo.Get(key, &entry)) {
      return;
    }
  }

  entry.AssertInvariant();

  if (entry.intolerant != 0) {
    // We've tried connecting at a higher range but failed, so try at the
    // version we haven't tried yet, unless we have reached the minimum.
    if (range.min < entry.intolerant) {
      range.max = entry.intolerant - 1;
    }
  }
}

PRErrorCode nsSSLIOLayerHelpers::getIntoleranceReason(
    const nsACString& hostName, int16_t port) {
  IntoleranceEntry entry;

  {
    nsCString key;
    getSiteKey(hostName, port, key);

    MutexAutoLock lock(mutex);
    if (!mTLSIntoleranceInfo.Get(key, &entry)) {
      return 0;
    }
  }

  entry.AssertInvariant();
  return entry.intoleranceReason;
}

bool nsSSLIOLayerHelpers::nsSSLIOLayerInitialized = false;
PRDescIdentity nsSSLIOLayerHelpers::nsSSLIOLayerIdentity;
PRDescIdentity nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity;
PRIOMethods nsSSLIOLayerHelpers::nsSSLIOLayerMethods;
PRIOMethods nsSSLIOLayerHelpers::nsSSLPlaintextLayerMethods;

static PRStatus nsSSLIOLayerClose(PRFileDesc* fd) {
  if (!fd) return PR_FAILURE;

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] Shutting down socket\n", (void*)fd));

  nsNSSSocketInfo* socketInfo = (nsNSSSocketInfo*)fd->secret;
  MOZ_ASSERT(socketInfo, "nsNSSSocketInfo was null for an fd");

  return socketInfo->CloseSocketAndDestroy();
}

PRStatus nsNSSSocketInfo::CloseSocketAndDestroy() {
  PRFileDesc* popped = PR_PopIOLayer(mFd, PR_TOP_IO_LAYER);
  MOZ_ASSERT(
      popped && popped->identity == nsSSLIOLayerHelpers::nsSSLIOLayerIdentity,
      "SSL Layer not on top of stack");

  // The plaintext layer is not always present - so it's not a fatal error if it
  // cannot be removed.
  // Note that PR_PopIOLayer may modify its stack, so a pointer returned by
  // PR_GetIdentitiesLayer may not point to what we think it points to after
  // calling PR_PopIOLayer. We must operate on the pointer returned by
  // PR_PopIOLayer.
  if (PR_GetIdentitiesLayer(mFd,
                            nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity)) {
    PRFileDesc* poppedPlaintext =
        PR_PopIOLayer(mFd, nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity);
    poppedPlaintext->dtor(poppedPlaintext);
  }

  // We need to clear the callback to make sure the ssl layer cannot call the
  // callback after mFD is nulled.
  if (StaticPrefs::network_ssl_tokens_cache_enabled()) {
    SSL_SetResumptionTokenCallback(mFd, nullptr, nullptr);
  }

  PRStatus status = mFd->methods->close(mFd);

  // the nsNSSSocketInfo instance can out-live the connection, so we need some
  // indication that the connection has been closed. mFd == nullptr is that
  // indication. This is needed, for example, when the connection is closed
  // before we have finished validating the server's certificate.
  mFd = nullptr;

  if (status != PR_SUCCESS) return status;

  popped->identity = PR_INVALID_IO_LAYER;
  NS_RELEASE_THIS();
  popped->dtor(popped);

  return PR_SUCCESS;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetEsniTxt(nsACString& aEsniTxt) {
  aEsniTxt = mEsniTxt;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetEsniTxt(const nsACString& aEsniTxt) {
  mEsniTxt = aEsniTxt;

  if (mEsniTxt.Length()) {
    nsAutoCString esniBin;
    if (NS_OK != Base64Decode(mEsniTxt, esniBin)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Error,
              ("[%p] Invalid ESNIKeys record. Couldn't base64 decode\n",
               (void*)mFd));
      return NS_OK;
    }

    if (SECSuccess !=
        SSL_EnableESNI(mFd, reinterpret_cast<const PRUint8*>(esniBin.get()),
                       esniBin.Length(), nullptr)) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Error,
              ("[%p] Invalid ESNIKeys record %s\n", (void*)mFd,
               PR_ErrorToName(PR_GetError())));
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetEchConfig(nsACString& aEchConfig) {
  aEchConfig = mEchConfig;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetEchConfig(const nsACString& aEchConfig) {
  mEchConfig = aEchConfig;

  if (mEchConfig.Length()) {
    if (SECSuccess !=
        SSL_SetClientEchConfigs(
            mFd, reinterpret_cast<const PRUint8*>(aEchConfig.BeginReading()),
            aEchConfig.Length())) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Error,
              ("[%p] Invalid EchConfig record %s\n", (void*)mFd,
               PR_ErrorToName(PR_GetError())));
      return NS_OK;
    }
    UpdateEchExtensionStatus(EchExtensionStatus::kReal);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetRetryEchConfig(nsACString& aEchConfig) {
  if (!mFd) {
    return NS_ERROR_FAILURE;
  }

  ScopedAutoSECItem retryConfigItem;
  SECStatus rv = SSL_GetEchRetryConfigs(mFd, &retryConfigItem);
  if (rv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  aEchConfig = nsCString(reinterpret_cast<const char*>(retryConfigItem.data),
                         retryConfigItem.len);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetPeerId(nsACString& aResult) {
  MutexAutoLock lock(mMutex);
  if (!mPeerId.IsEmpty()) {
    aResult.Assign(mPeerId);
    return NS_OK;
  }

  if (mProviderFlags &
      nsISocketProvider::ANONYMOUS_CONNECT) {  // See bug 466080
    mPeerId.AppendLiteral("anon:");
  }
  if (mProviderFlags & nsISocketProvider::NO_PERMANENT_STORAGE) {
    mPeerId.AppendLiteral("private:");
  }
  if (mProviderFlags & nsISocketProvider::BE_CONSERVATIVE) {
    mPeerId.AppendLiteral("beConservative:");
  }

  mPeerId.AppendPrintf("tlsflags0x%08x:", mProviderTlsFlags);

  mPeerId.Append(mHostName);
  mPeerId.Append(':');
  mPeerId.AppendInt(GetPort());
  nsAutoCString suffix;
  mOriginAttributes.CreateSuffix(suffix);
  mPeerId.Append(suffix);

  aResult.Assign(mPeerId);
  return NS_OK;
}

nsresult nsNSSSocketInfo::SetResumptionTokenFromExternalCache() {
  if (!StaticPrefs::network_ssl_tokens_cache_enabled()) {
    return NS_OK;
  }

  if (!mFd) {
    return NS_ERROR_FAILURE;
  }

  // If SSL_NO_CACHE option was set, we must not use the cache
  PRIntn val;
  if (SSL_OptionGet(mFd, SSL_NO_CACHE, &val) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  if (val != 0) {
    return NS_OK;
  }

  nsTArray<uint8_t> token;
  nsAutoCString peerId;
  nsresult rv = GetPeerId(peerId);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = mozilla::net::SSLTokensCache::Get(peerId, token);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      // It's ok if we can't find the token.
      return NS_OK;
    }

    return rv;
  }

  SECStatus srv = SSL_SetResumptionToken(mFd, token.Elements(), token.Length());
  if (srv == SECFailure) {
    PRErrorCode error = PR_GetError();
    mozilla::net::SSLTokensCache::Remove(peerId);
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("Setting token failed with NSS error %d [id=%s]", error,
             PromiseFlatCString(peerId).get()));
    // We don't consider SSL_ERROR_BAD_RESUMPTION_TOKEN_ERROR as a hard error,
    // since this error means this token is just expired or can't be decoded
    // correctly.
    if (error == SSL_ERROR_BAD_RESUMPTION_TOKEN_ERROR) {
      return NS_OK;
    }

    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

#if defined(DEBUG_SSL_VERBOSE) && defined(DUMP_BUFFER)
// Dumps a (potentially binary) buffer using SSM_DEBUG.  (We could have used
// the version in ssltrace.c, but that's specifically tailored to SSLTRACE.)
#  define DUMPBUF_LINESIZE 24
static void nsDumpBuffer(unsigned char* buf, int len) {
  char hexbuf[DUMPBUF_LINESIZE * 3 + 1];
  char chrbuf[DUMPBUF_LINESIZE + 1];
  static const char* hex = "0123456789abcdef";
  int i = 0;
  int l = 0;
  char ch;
  char* c;
  char* h;
  if (len == 0) return;
  hexbuf[DUMPBUF_LINESIZE * 3] = '\0';
  chrbuf[DUMPBUF_LINESIZE] = '\0';
  (void)memset(hexbuf, 0x20, DUMPBUF_LINESIZE * 3);
  (void)memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
  h = hexbuf;
  c = chrbuf;

  while (i < len) {
    ch = buf[i];

    if (l == DUMPBUF_LINESIZE) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("%s%s\n", hexbuf, chrbuf));
      (void)memset(hexbuf, 0x20, DUMPBUF_LINESIZE * 3);
      (void)memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
      h = hexbuf;
      c = chrbuf;
      l = 0;
    }

    // Convert a character to hex.
    *h++ = hex[(ch >> 4) & 0xf];
    *h++ = hex[ch & 0xf];
    h++;

    // Put the character (if it's printable) into the character buffer.
    if ((ch >= 0x20) && (ch <= 0x7e)) {
      *c++ = ch;
    } else {
      *c++ = '.';
    }
    i++;
    l++;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("%s%s\n", hexbuf, chrbuf));
}

#  define DEBUG_DUMP_BUFFER(buf, len) nsDumpBuffer(buf, len)
#else
#  define DEBUG_DUMP_BUFFER(buf, len)
#endif

namespace {

uint32_t tlsIntoleranceTelemetryBucket(PRErrorCode err) {
  // returns a numeric code for where we track various errors in telemetry
  // only errors that cause version fallback are tracked,
  // so this is also used to determine which errors can cause version fallback
  switch (err) {
    case SSL_ERROR_BAD_MAC_ALERT:
      return 1;
    case SSL_ERROR_BAD_MAC_READ:
      return 2;
    case SSL_ERROR_HANDSHAKE_FAILURE_ALERT:
      return 3;
    case SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT:
      return 4;
    case SSL_ERROR_ILLEGAL_PARAMETER_ALERT:
      return 6;
    case SSL_ERROR_NO_CYPHER_OVERLAP:
      return 7;
    case SSL_ERROR_UNSUPPORTED_VERSION:
      return 10;
    case SSL_ERROR_PROTOCOL_VERSION_ALERT:
      return 11;
    case SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE:
      return 13;
    case SSL_ERROR_DECODE_ERROR_ALERT:
      return 14;
    case PR_CONNECT_RESET_ERROR:
      return 16;
    case PR_END_OF_FILE_ERROR:
      return 17;
    case SSL_ERROR_INTERNAL_ERROR_ALERT:
      return 18;
    default:
      return 0;
  }
}

bool retryDueToTLSIntolerance(PRErrorCode err, nsNSSSocketInfo* socketInfo) {
  // This function is supposed to decide which error codes should
  // be used to conclude server is TLS intolerant.
  // Note this only happens during the initial SSL handshake.

  if (StaticPrefs::security_tls_ech_disable_grease_on_fallback() &&
      socketInfo->GetEchExtensionStatus() == EchExtensionStatus::kGREASE) {
    // Don't record any intolerances if we used ECH GREASE but force a retry.
    return true;
  }

  SSLVersionRange range = socketInfo->GetTLSVersionRange();
  nsSSLIOLayerHelpers& helpers = socketInfo->SharedState().IOLayerHelpers();

  if (err == SSL_ERROR_UNSUPPORTED_VERSION &&
      range.min == SSL_LIBRARY_VERSION_TLS_1_0) {
    socketInfo->SetSecurityState(nsIWebProgressListener::STATE_IS_INSECURE |
                                 nsIWebProgressListener::STATE_USES_SSL_3);
  }

  // NSS will return SSL_ERROR_RX_MALFORMED_SERVER_HELLO if anti-downgrade
  // detected the downgrade.
  if (err == SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT ||
      err == SSL_ERROR_RX_MALFORMED_SERVER_HELLO) {
    // This is a clear signal that we've fallen back too many versions.  Treat
    // this as a hard failure, but forget any intolerance so that later attempts
    // don't use this version (i.e., range.max) and trigger the error again.

    // First, track the original cause of the version fallback.  This uses the
    // same buckets as the telemetry below, except that bucket 0 will include
    // all cases where there wasn't an original reason.
    PRErrorCode originalReason = helpers.getIntoleranceReason(
        socketInfo->GetHostName(), socketInfo->GetPort());
    Telemetry::Accumulate(Telemetry::SSL_VERSION_FALLBACK_INAPPROPRIATE,
                          tlsIntoleranceTelemetryBucket(originalReason));

    helpers.forgetIntolerance(socketInfo->GetHostName(), socketInfo->GetPort());

    return false;
  }

  // When not using a proxy we'll see a connection reset error.
  // When using a proxy, we'll see an end of file error.

  // Don't allow STARTTLS connections to fall back on connection resets or
  // EOF.
  if ((err == PR_CONNECT_RESET_ERROR || err == PR_END_OF_FILE_ERROR) &&
      socketInfo->GetForSTARTTLS()) {
    return false;
  }

  uint32_t reason = tlsIntoleranceTelemetryBucket(err);
  if (reason == 0) {
    return false;
  }

  Telemetry::HistogramID pre;
  Telemetry::HistogramID post;
  switch (range.max) {
    case SSL_LIBRARY_VERSION_TLS_1_3:
      pre = Telemetry::SSL_TLS13_INTOLERANCE_REASON_PRE;
      post = Telemetry::SSL_TLS13_INTOLERANCE_REASON_POST;
      break;
    case SSL_LIBRARY_VERSION_TLS_1_2:
      pre = Telemetry::SSL_TLS12_INTOLERANCE_REASON_PRE;
      post = Telemetry::SSL_TLS12_INTOLERANCE_REASON_POST;
      break;
    case SSL_LIBRARY_VERSION_TLS_1_1:
      pre = Telemetry::SSL_TLS11_INTOLERANCE_REASON_PRE;
      post = Telemetry::SSL_TLS11_INTOLERANCE_REASON_POST;
      break;
    case SSL_LIBRARY_VERSION_TLS_1_0:
      pre = Telemetry::SSL_TLS10_INTOLERANCE_REASON_PRE;
      post = Telemetry::SSL_TLS10_INTOLERANCE_REASON_POST;
      break;
    default:
      MOZ_CRASH("impossible TLS version");
      return false;
  }

  // The difference between _PRE and _POST represents how often we avoided
  // TLS intolerance fallback due to remembered tolerance.
  Telemetry::Accumulate(pre, reason);

  if (!helpers.rememberIntolerantAtVersion(socketInfo->GetHostName(),
                                           socketInfo->GetPort(), range.min,
                                           range.max, err)) {
    return false;
  }

  Telemetry::Accumulate(post, reason);

  return true;
}

// Ensure that we haven't added too many errors to fit.
static_assert((SSL_ERROR_END_OF_LIST - SSL_ERROR_BASE) <= 256,
              "too many SSL errors");
static_assert((SEC_ERROR_END_OF_LIST - SEC_ERROR_BASE) <= 256,
              "too many SEC errors");
static_assert((PR_MAX_ERROR - PR_NSPR_ERROR_BASE) <= 128,
              "too many NSPR errors");
static_assert((mozilla::pkix::ERROR_BASE - mozilla::pkix::END_OF_LIST) < 31,
              "too many moz::pkix errors");

static void reportHandshakeResult(int32_t bytesTransferred, bool wasReading,
                                  PRErrorCode err,
                                  EchExtensionStatus aEchExtensionStatus) {
  uint32_t bucket;

  // A negative bytesTransferred or a 0 read are errors.
  if (bytesTransferred > 0) {
    bucket = 0;
  } else if ((bytesTransferred == 0) && !wasReading) {
    // PR_Write() is defined to never return 0, but let's make sure.
    // https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSPR/Reference/PR_Write.
    MOZ_ASSERT(false);
    bucket = 671;
  } else if (IS_SSL_ERROR(err)) {
    bucket = err - SSL_ERROR_BASE;
    MOZ_ASSERT(bucket > 0);  // SSL_ERROR_EXPORT_ONLY_SERVER isn't used.
  } else if (IS_SEC_ERROR(err)) {
    bucket = (err - SEC_ERROR_BASE) + 256;
  } else if ((err >= PR_NSPR_ERROR_BASE) && (err < PR_MAX_ERROR)) {
    bucket = (err - PR_NSPR_ERROR_BASE) + 512;
  } else if ((err >= mozilla::pkix::ERROR_BASE) &&
             (err < mozilla::pkix::ERROR_LIMIT)) {
    bucket = (err - mozilla::pkix::ERROR_BASE) + 640;
  } else {
    bucket = 671;
  }

  Telemetry::HistogramID result_histogram;
  switch (aEchExtensionStatus) {
    case EchExtensionStatus::kNotPresent:
      result_histogram = Telemetry::SSL_HANDSHAKE_RESULT;
      break;
    case EchExtensionStatus::kGREASE:
      result_histogram = Telemetry::SSL_HANDSHAKE_RESULT_ECH_GREASE;
      break;
    case EchExtensionStatus::kReal:
      result_histogram = Telemetry::SSL_HANDSHAKE_RESULT_ECH;
      break;
  }
  Telemetry::Accumulate(result_histogram, bucket);
}

int32_t checkHandshake(int32_t bytesTransfered, bool wasReading,
                       PRFileDesc* ssl_layer_fd, nsNSSSocketInfo* socketInfo) {
  const PRErrorCode originalError = PR_GetError();
  PRErrorCode err = originalError;

  // This is where we work around all of those SSL servers that don't
  // conform to the SSL spec and shutdown a connection when we request
  // SSL v3.1 (aka TLS).  The spec says the client says what version
  // of the protocol we're willing to perform, in our case SSL v3.1
  // In its response, the server says which version it wants to perform.
  // Many servers out there only know how to do v3.0.  Next, we're supposed
  // to send back the version of the protocol we requested (ie v3.1).  At
  // this point many servers's implementations are broken and they shut
  // down the connection when they don't see the version they sent back.
  // This is supposed to prevent a man in the middle from forcing one
  // side to dumb down to a lower level of the protocol.  Unfortunately,
  // there are enough broken servers out there that such a gross work-around
  // is necessary.  :(

  // Do NOT assume TLS intolerance on a closed connection after bad cert ui was
  // shown. Simply retry. This depends on the fact that Cert UI will not be
  // shown again, should the user override the bad cert.

  bool handleHandshakeResultNow = socketInfo->IsHandshakePending();

  bool wantRetry = false;

  if (0 > bytesTransfered) {
    if (handleHandshakeResultNow) {
      if (PR_WOULD_BLOCK_ERROR == err) {
        PR_SetError(err, 0);
        return bytesTransfered;
      }

      wantRetry = retryDueToTLSIntolerance(err, socketInfo);
    }

    // This is the common place where we trigger non-cert-errors on a SSL
    // socket. This might be reached at any time of the connection.
    //
    // IsCanceled() is backed by an atomic boolean. It will only ever go from
    // false to true, so we will never erroneously not call SetCanceled here. We
    // could in theory overwrite a previously-set error code, but we'll always
    // have some sort of error.
    if (!wantRetry && mozilla::psm::IsNSSErrorCode(err) &&
        !socketInfo->IsCanceled()) {
      socketInfo->SetCanceled(err);
    }
  } else if (wasReading && 0 == bytesTransfered) {
    // zero bytes on reading, socket closed
    if (handleHandshakeResultNow) {
      wantRetry = retryDueToTLSIntolerance(PR_END_OF_FILE_ERROR, socketInfo);
    }
  }

  if (wantRetry) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] checkHandshake: will retry with lower max TLS version\n",
             ssl_layer_fd));
    // We want to cause the network layer to retry the connection.
    err = PR_CONNECT_RESET_ERROR;
    if (wasReading) bytesTransfered = -1;
  }

  // TLS intolerant servers only cause the first transfer to fail, so let's
  // set the HandshakePending attribute to false so that we don't try the logic
  // above again in a subsequent transfer.
  if (handleHandshakeResultNow) {
    // Report the result once for each handshake. Note that this does not
    // get handshakes which are cancelled before any reads or writes
    // happen.
    reportHandshakeResult(bytesTransfered, wasReading, originalError,
                          socketInfo->GetEchExtensionStatus());
    socketInfo->SetHandshakeNotPending();
  }

  if (bytesTransfered < 0) {
    // Remember that we encountered an error so that getSocketInfoIfRunning
    // will correctly cause us to fail if another part of Gecko
    // (erroneously) calls an I/O function (PR_Send/PR_Recv/etc.) again on
    // this socket. Note that we use the original error because if we use
    // PR_CONNECT_RESET_ERROR, we'll repeated try to reconnect.
    if (originalError != PR_WOULD_BLOCK_ERROR && !socketInfo->IsCanceled()) {
      socketInfo->SetCanceled(originalError);
    }
    PR_SetError(err, 0);
  }

  return bytesTransfered;
}

}  // namespace

static int16_t nsSSLIOLayerPoll(PRFileDesc* fd, int16_t in_flags,
                                int16_t* out_flags) {
  if (!out_flags) {
    NS_WARNING("nsSSLIOLayerPoll called with null out_flags");
    return 0;
  }

  *out_flags = 0;

  nsNSSSocketInfo* socketInfo =
      getSocketInfoIfRunning(fd, not_reading_or_writing);

  if (!socketInfo) {
    // If we get here, it is probably because certificate validation failed
    // and this is the first I/O operation after the failure.
    MOZ_LOG(
        gPIPNSSLog, LogLevel::Debug,
        ("[%p] polling SSL socket right after certificate verification failed "
         "or NSS shutdown or SDR logout %d\n",
         fd, (int)in_flags));

    MOZ_ASSERT(in_flags & PR_POLL_EXCEPT,
               "Caller did not poll for EXCEPT (canceled)");
    // Since this poll method cannot return errors, we want the caller to call
    // PR_Send/PR_Recv right away to get the error, so we tell that we are
    // ready for whatever I/O they are asking for. (See getSocketInfoIfRunning).
    *out_flags = in_flags | PR_POLL_EXCEPT;  // see also bug 480619
    return in_flags;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
          (socketInfo->IsWaitingForCertVerification()
               ? "[%p] polling SSL socket during certificate verification "
                 "using lower %d\n"
               : "[%p] poll SSL socket using lower %d\n",
           fd, (int)in_flags));

  // We want the handshake to continue during certificate validation, so we
  // don't need to do anything special here. libssl automatically blocks when
  // it reaches any point that would be unsafe to send/receive something before
  // cert validation is complete.
  int16_t result = fd->lower->methods->poll(fd->lower, in_flags, out_flags);
  MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
          ("[%p] poll SSL socket returned %d\n", (void*)fd, (int)result));
  return result;
}

nsSSLIOLayerHelpers::nsSSLIOLayerHelpers(uint32_t aTlsFlags)
    : mTreatUnsafeNegotiationAsBroken(false),
      mTLSIntoleranceInfo(),
      mVersionFallbackLimit(SSL_LIBRARY_VERSION_TLS_1_0),
      mutex("nsSSLIOLayerHelpers.mutex"),
      mTlsFlags(aTlsFlags) {}

// PSMAvailable and PSMAvailable64 are reachable, but they're unimplemented in
// PSM, so we set an error and return -1.
static int32_t PSMAvailable(PRFileDesc*) {
  PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
  return -1;
}

static int64_t PSMAvailable64(PRFileDesc*) {
  PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
  return -1;
}

static PRStatus PSMGetsockname(PRFileDesc* fd, PRNetAddr* addr) {
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing)) return PR_FAILURE;

  return fd->lower->methods->getsockname(fd->lower, addr);
}

static PRStatus PSMGetpeername(PRFileDesc* fd, PRNetAddr* addr) {
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing)) return PR_FAILURE;

  return fd->lower->methods->getpeername(fd->lower, addr);
}

static PRStatus PSMGetsocketoption(PRFileDesc* fd, PRSocketOptionData* data) {
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing)) return PR_FAILURE;

  return fd->lower->methods->getsocketoption(fd, data);
}

static PRStatus PSMSetsocketoption(PRFileDesc* fd,
                                   const PRSocketOptionData* data) {
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing)) return PR_FAILURE;

  return fd->lower->methods->setsocketoption(fd, data);
}

static int32_t PSMRecv(PRFileDesc* fd, void* buf, int32_t amount, int flags,
                       PRIntervalTime timeout) {
  nsNSSSocketInfo* socketInfo = getSocketInfoIfRunning(fd, reading);
  if (!socketInfo) return -1;

  if (flags != PR_MSG_PEEK && flags != 0) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return -1;
  }

  int32_t bytesRead =
      fd->lower->methods->recv(fd->lower, buf, amount, flags, timeout);

  MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
          ("[%p] read %d bytes\n", (void*)fd, bytesRead));

#ifdef DEBUG_SSL_VERBOSE
  DEBUG_DUMP_BUFFER((unsigned char*)buf, bytesRead);
#endif

  return checkHandshake(bytesRead, true, fd, socketInfo);
}

static int32_t PSMSend(PRFileDesc* fd, const void* buf, int32_t amount,
                       int flags, PRIntervalTime timeout) {
  nsNSSSocketInfo* socketInfo = getSocketInfoIfRunning(fd, writing);
  if (!socketInfo) return -1;

  if (flags != 0) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return -1;
  }

#ifdef DEBUG_SSL_VERBOSE
  DEBUG_DUMP_BUFFER((unsigned char*)buf, amount);
#endif

  if (socketInfo->IsShortWritePending() && amount > 0) {
    // We got "SSL short write" last time, try to flush the pending byte.
#ifdef DEBUG
    socketInfo->CheckShortWrittenBuffer(static_cast<const unsigned char*>(buf),
                                        amount);
#endif

    buf = socketInfo->GetShortWritePendingByteRef();
    amount = 1;

    MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
            ("[%p] pushing 1 byte after SSL short write", fd));
  }

  int32_t bytesWritten =
      fd->lower->methods->send(fd->lower, buf, amount, flags, timeout);

  // NSS indicates that it can't write all requested data (due to network
  // congestion, for example) by returning either one less than the amount
  // of data requested or 16383, if the requested amount is greater than
  // 16384. We refer to this as a "short write". If we simply returned
  // the amount that NSS did write, the layer above us would then call
  // PSMSend with a very small amount of data (often 1). This is inefficient
  // and can lead to alternating between sending large packets and very small
  // packets. To prevent this, we alert the layer calling us that the operation
  // would block and that it should be retried later, with the same data.
  // When it does, we tell NSS to write the remaining byte it didn't write
  // in the previous call. We then return the total number of bytes written,
  // which is the number that caused the short write plus the additional byte
  // we just wrote out.

  // The 16384 value is based on libssl's maximum buffer size:
  //    MAX_FRAGMENT_LENGTH - 1
  //
  // It's in a private header, though, filed bug 1394822 to expose it.
  static const int32_t kShortWrite16k = 16383;

  if ((amount > 1 && bytesWritten == (amount - 1)) ||
      (amount > kShortWrite16k && bytesWritten == kShortWrite16k)) {
    // This is indication of an "SSL short write", block to force retry.
    socketInfo->SetShortWritePending(
        bytesWritten + 1,  // The amount to return after the flush
        *(static_cast<const unsigned char*>(buf) + bytesWritten));

    MOZ_LOG(
        gPIPNSSLog, LogLevel::Verbose,
        ("[%p] indicated SSL short write for %d bytes (written just %d bytes)",
         fd, amount, bytesWritten));

    bytesWritten = -1;
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);

#ifdef DEBUG
    socketInfo->RememberShortWrittenBuffer(
        static_cast<const unsigned char*>(buf));
#endif

  } else if (socketInfo->IsShortWritePending() && bytesWritten == 1) {
    // We have now flushed all pending data in the SSL socket
    // after the indicated short write.  Tell the upper layer
    // it has sent all its data now.
    MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
            ("[%p] finished SSL short write", fd));

    bytesWritten = socketInfo->ResetShortWritePending();
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
          ("[%p] wrote %d bytes\n", fd, bytesWritten));

  return checkHandshake(bytesWritten, false, fd, socketInfo);
}

static PRStatus PSMBind(PRFileDesc* fd, const PRNetAddr* addr) {
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing)) return PR_FAILURE;

  return fd->lower->methods->bind(fd->lower, addr);
}

static int32_t nsSSLIOLayerRead(PRFileDesc* fd, void* buf, int32_t amount) {
  return PSMRecv(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}

static int32_t nsSSLIOLayerWrite(PRFileDesc* fd, const void* buf,
                                 int32_t amount) {
  return PSMSend(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}

static PRStatus PSMConnectcontinue(PRFileDesc* fd, int16_t out_flags) {
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing)) {
    return PR_FAILURE;
  }

  return fd->lower->methods->connectcontinue(fd, out_flags);
}

namespace {

class PrefObserver : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  explicit PrefObserver(nsSSLIOLayerHelpers* aOwner) : mOwner(aOwner) {}

 protected:
  virtual ~PrefObserver() = default;

 private:
  nsSSLIOLayerHelpers* mOwner;
};

}  // unnamed namespace

NS_IMPL_ISUPPORTS(PrefObserver, nsIObserver)

NS_IMETHODIMP
PrefObserver::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* someData) {
  if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    NS_ConvertUTF16toUTF8 prefName(someData);

    if (prefName.EqualsLiteral(
            "security.ssl.treat_unsafe_negotiation_as_broken")) {
      bool enabled;
      Preferences::GetBool("security.ssl.treat_unsafe_negotiation_as_broken",
                           &enabled);
      mOwner->setTreatUnsafeNegotiationAsBroken(enabled);
    } else if (prefName.EqualsLiteral("security.tls.version.fallback-limit")) {
      mOwner->loadVersionFallbackLimit();
    } else if (prefName.EqualsLiteral("security.tls.insecure_fallback_hosts")) {
      // Changes to the allowlist on the public side will update the pref.
      // Don't propagate the changes to the private side.
      if (mOwner->isPublic()) {
        mOwner->initInsecureFallbackSites();
      }
    }
  }
  return NS_OK;
}

static int32_t PlaintextRecv(PRFileDesc* fd, void* buf, int32_t amount,
                             int flags, PRIntervalTime timeout) {
  // The shutdownlocker is not needed here because it will already be
  // held higher in the stack
  nsNSSSocketInfo* socketInfo = nullptr;

  int32_t bytesRead =
      fd->lower->methods->recv(fd->lower, buf, amount, flags, timeout);
  if (fd->identity == nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity)
    socketInfo = (nsNSSSocketInfo*)fd->secret;

  if ((bytesRead > 0) && socketInfo)
    socketInfo->AddPlaintextBytesRead(bytesRead);
  return bytesRead;
}

nsSSLIOLayerHelpers::~nsSSLIOLayerHelpers() {
  // mPrefObserver will only be set if this->Init was called. The GTest tests
  // do not call Init.
  if (mPrefObserver) {
    Preferences::RemoveObserver(
        mPrefObserver, "security.ssl.treat_unsafe_negotiation_as_broken");
    Preferences::RemoveObserver(mPrefObserver,
                                "security.tls.version.fallback-limit");
    Preferences::RemoveObserver(mPrefObserver,
                                "security.tls.insecure_fallback_hosts");
  }
}

template <typename R, R return_value, typename... Args>
static R InvalidPRIOMethod(Args...) {
  MOZ_ASSERT_UNREACHABLE("I/O method is invalid");
  PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
  return return_value;
}

nsresult nsSSLIOLayerHelpers::Init() {
  if (!nsSSLIOLayerInitialized) {
    MOZ_ASSERT(NS_IsMainThread());
    nsSSLIOLayerInitialized = true;
    nsSSLIOLayerIdentity = PR_GetUniqueIdentity("NSS layer");
    nsSSLIOLayerMethods = *PR_GetDefaultIOMethods();

    nsSSLIOLayerMethods.fsync =
        InvalidPRIOMethod<PRStatus, PR_FAILURE, PRFileDesc*>;
    nsSSLIOLayerMethods.seek =
        InvalidPRIOMethod<int32_t, -1, PRFileDesc*, int32_t, PRSeekWhence>;
    nsSSLIOLayerMethods.seek64 =
        InvalidPRIOMethod<int64_t, -1, PRFileDesc*, int64_t, PRSeekWhence>;
    nsSSLIOLayerMethods.fileInfo =
        InvalidPRIOMethod<PRStatus, PR_FAILURE, PRFileDesc*, PRFileInfo*>;
    nsSSLIOLayerMethods.fileInfo64 =
        InvalidPRIOMethod<PRStatus, PR_FAILURE, PRFileDesc*, PRFileInfo64*>;
    nsSSLIOLayerMethods.writev =
        InvalidPRIOMethod<int32_t, -1, PRFileDesc*, const PRIOVec*, int32_t,
                          PRIntervalTime>;
    nsSSLIOLayerMethods.accept =
        InvalidPRIOMethod<PRFileDesc*, nullptr, PRFileDesc*, PRNetAddr*,
                          PRIntervalTime>;
    nsSSLIOLayerMethods.listen =
        InvalidPRIOMethod<PRStatus, PR_FAILURE, PRFileDesc*, int>;
    nsSSLIOLayerMethods.shutdown =
        InvalidPRIOMethod<PRStatus, PR_FAILURE, PRFileDesc*, int>;
    nsSSLIOLayerMethods.recvfrom =
        InvalidPRIOMethod<int32_t, -1, PRFileDesc*, void*, int32_t, int,
                          PRNetAddr*, PRIntervalTime>;
    nsSSLIOLayerMethods.sendto =
        InvalidPRIOMethod<int32_t, -1, PRFileDesc*, const void*, int32_t, int,
                          const PRNetAddr*, PRIntervalTime>;
    nsSSLIOLayerMethods.acceptread =
        InvalidPRIOMethod<int32_t, -1, PRFileDesc*, PRFileDesc**, PRNetAddr**,
                          void*, int32_t, PRIntervalTime>;
    nsSSLIOLayerMethods.transmitfile =
        InvalidPRIOMethod<int32_t, -1, PRFileDesc*, PRFileDesc*, const void*,
                          int32_t, PRTransmitFileFlags, PRIntervalTime>;
    nsSSLIOLayerMethods.sendfile =
        InvalidPRIOMethod<int32_t, -1, PRFileDesc*, PRSendFileData*,
                          PRTransmitFileFlags, PRIntervalTime>;

    nsSSLIOLayerMethods.available = PSMAvailable;
    nsSSLIOLayerMethods.available64 = PSMAvailable64;
    nsSSLIOLayerMethods.getsockname = PSMGetsockname;
    nsSSLIOLayerMethods.getpeername = PSMGetpeername;
    nsSSLIOLayerMethods.getsocketoption = PSMGetsocketoption;
    nsSSLIOLayerMethods.setsocketoption = PSMSetsocketoption;
    nsSSLIOLayerMethods.recv = PSMRecv;
    nsSSLIOLayerMethods.send = PSMSend;
    nsSSLIOLayerMethods.connectcontinue = PSMConnectcontinue;
    nsSSLIOLayerMethods.bind = PSMBind;

    nsSSLIOLayerMethods.connect = nsSSLIOLayerConnect;
    nsSSLIOLayerMethods.close = nsSSLIOLayerClose;
    nsSSLIOLayerMethods.write = nsSSLIOLayerWrite;
    nsSSLIOLayerMethods.read = nsSSLIOLayerRead;
    nsSSLIOLayerMethods.poll = nsSSLIOLayerPoll;

    nsSSLPlaintextLayerIdentity = PR_GetUniqueIdentity("Plaintxext PSM layer");
    nsSSLPlaintextLayerMethods = *PR_GetDefaultIOMethods();
    nsSSLPlaintextLayerMethods.recv = PlaintextRecv;
  }

  loadVersionFallbackLimit();

  // non main thread helpers will need to use defaults
  if (NS_IsMainThread()) {
    bool enabled = false;
    Preferences::GetBool("security.ssl.treat_unsafe_negotiation_as_broken",
                         &enabled);
    setTreatUnsafeNegotiationAsBroken(enabled);

    initInsecureFallbackSites();

    mPrefObserver = new PrefObserver(this);
    Preferences::AddStrongObserver(
        mPrefObserver, "security.ssl.treat_unsafe_negotiation_as_broken");
    Preferences::AddStrongObserver(mPrefObserver,
                                   "security.tls.version.fallback-limit");
    Preferences::AddStrongObserver(mPrefObserver,
                                   "security.tls.insecure_fallback_hosts");
  } else {
    MOZ_ASSERT(mTlsFlags, "Only per socket version can ignore prefs");
  }

  return NS_OK;
}

void nsSSLIOLayerHelpers::loadVersionFallbackLimit() {
  // see nsNSSComponent::SetEnabledTLSVersions for pref handling rules
  uint32_t limit = 3;  // TLS 1.2

  if (NS_IsMainThread()) {
    limit = Preferences::GetUint("security.tls.version.fallback-limit",
                                 3);  // 3 = TLS 1.2
  }

  // set fallback limit if it is set in the tls flags
  uint32_t tlsFlagsFallbackLimit = getTLSProviderFlagFallbackLimit(mTlsFlags);

  if (tlsFlagsFallbackLimit) {
    limit = tlsFlagsFallbackLimit;
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("loadVersionFallbackLimit overriden by tlsFlags %d\n", limit));
  }

  SSLVersionRange defaults = {SSL_LIBRARY_VERSION_TLS_1_2,
                              SSL_LIBRARY_VERSION_TLS_1_2};
  SSLVersionRange filledInRange;
  nsNSSComponent::FillTLSVersionRange(filledInRange, limit, limit, defaults);
  if (filledInRange.max < SSL_LIBRARY_VERSION_TLS_1_2) {
    filledInRange.max = SSL_LIBRARY_VERSION_TLS_1_2;
  }

  mVersionFallbackLimit = filledInRange.max;
}

void nsSSLIOLayerHelpers::clearStoredData() {
  MOZ_ASSERT(NS_IsMainThread());
  initInsecureFallbackSites();

  MutexAutoLock lock(mutex);
  mTLSIntoleranceInfo.Clear();
}

void nsSSLIOLayerHelpers::setInsecureFallbackSites(const nsCString& str) {
  MutexAutoLock lock(mutex);

  mInsecureFallbackSites.Clear();

  for (const nsACString& host : nsCCharSeparatedTokenizer(str, ',').ToRange()) {
    if (!host.IsEmpty()) {
      mInsecureFallbackSites.PutEntry(host);
    }
  }
}

void nsSSLIOLayerHelpers::initInsecureFallbackSites() {
  MOZ_ASSERT(NS_IsMainThread());
  nsAutoCString insecureFallbackHosts;
  Preferences::GetCString("security.tls.insecure_fallback_hosts",
                          insecureFallbackHosts);
  setInsecureFallbackSites(insecureFallbackHosts);
}

bool nsSSLIOLayerHelpers::isPublic() const {
  return this == &PublicSSLState()->IOLayerHelpers();
}

class FallbackPrefRemover final : public Runnable {
 public:
  explicit FallbackPrefRemover(const nsACString& aHost)
      : mozilla::Runnable("FallbackPrefRemover"), mHost(aHost) {}
  NS_IMETHOD Run() override;

 private:
  nsCString mHost;
};

NS_IMETHODIMP
FallbackPrefRemover::Run() {
  MOZ_ASSERT(NS_IsMainThread());
  nsAutoCString oldValue;
  Preferences::GetCString("security.tls.insecure_fallback_hosts", oldValue);
  nsCString newValue;
  for (const nsACString& host :
       nsCCharSeparatedTokenizer(oldValue, ',').ToRange()) {
    if (host.Equals(mHost)) {
      continue;
    }
    if (!newValue.IsEmpty()) {
      newValue.Append(',');
    }
    newValue.Append(host);
  }
  Preferences::SetCString("security.tls.insecure_fallback_hosts", newValue);
  return NS_OK;
}

void nsSSLIOLayerHelpers::removeInsecureFallbackSite(const nsACString& hostname,
                                                     uint16_t port) {
  forgetIntolerance(hostname, port);
  {
    MutexAutoLock lock(mutex);
    if (!mInsecureFallbackSites.Contains(hostname)) {
      return;
    }
    mInsecureFallbackSites.RemoveEntry(hostname);
  }
  if (!isPublic()) {
    return;
  }
  RefPtr<Runnable> runnable = new FallbackPrefRemover(hostname);
  if (NS_IsMainThread()) {
    runnable->Run();
  } else {
    NS_DispatchToMainThread(runnable);
  }
}

bool nsSSLIOLayerHelpers::isInsecureFallbackSite(const nsACString& hostname) {
  MutexAutoLock lock(mutex);
  return mInsecureFallbackSites.Contains(hostname);
}

void nsSSLIOLayerHelpers::setTreatUnsafeNegotiationAsBroken(bool broken) {
  MutexAutoLock lock(mutex);
  mTreatUnsafeNegotiationAsBroken = broken;
}

bool nsSSLIOLayerHelpers::treatUnsafeNegotiationAsBroken() {
  MutexAutoLock lock(mutex);
  return mTreatUnsafeNegotiationAsBroken;
}

nsresult nsSSLIOLayerNewSocket(int32_t family, const char* host, int32_t port,
                               nsIProxyInfo* proxy,
                               const OriginAttributes& originAttributes,
                               PRFileDesc** fd, nsISupports** info,
                               bool forSTARTTLS, uint32_t flags,
                               uint32_t tlsFlags) {
  PRFileDesc* sock = PR_OpenTCPSocket(family);
  if (!sock) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv =
      nsSSLIOLayerAddToSocket(family, host, port, proxy, originAttributes, sock,
                              info, forSTARTTLS, flags, tlsFlags);
  if (NS_FAILED(rv)) {
    PR_Close(sock);
    return rv;
  }

  *fd = sock;
  return NS_OK;
}

// Possible behaviors for choosing a cert for client auth.
enum class UserCertChoice {
  // Ask the user to choose a cert.
  Ask = 0,
  // Automatically choose a cert.
  Auto = 1,
};

// Returns the most appropriate user cert choice based on the value of the
// security.default_personal_cert preference.
UserCertChoice nsGetUserCertChoice() {
  nsAutoCString value;
  nsresult rv =
      Preferences::GetCString("security.default_personal_cert", value);
  if (NS_FAILED(rv)) {
    return UserCertChoice::Ask;
  }

  // There are three cases for what the preference could be set to:
  //   1. "Select Automatically" -> Auto.
  //   2. "Ask Every Time" -> Ask.
  //   3. Something else -> Ask. This might be a nickname from a migrated cert,
  //      but we no longer support this case.
  return value.EqualsLiteral("Select Automatically") ? UserCertChoice::Auto
                                                     : UserCertChoice::Ask;
}

static bool hasExplicitKeyUsageNonRepudiation(CERTCertificate* cert) {
  // There is no extension, v1 or v2 certificate
  if (!cert->extensions) return false;

  SECStatus srv;
  SECItem keyUsageItem;
  keyUsageItem.data = nullptr;

  srv = CERT_FindKeyUsageExtension(cert, &keyUsageItem);
  if (srv == SECFailure) return false;

  unsigned char keyUsage = keyUsageItem.data[0];
  PORT_Free(keyUsageItem.data);

  return !!(keyUsage & KU_NON_REPUDIATION);
}

ClientAuthInfo::ClientAuthInfo(const nsACString& hostName,
                               const OriginAttributes& originAttributes,
                               int32_t port, uint32_t providerFlags,
                               uint32_t providerTlsFlags)
    : mHostName(hostName),
      mOriginAttributes(originAttributes),
      mPort(port),
      mProviderFlags(providerFlags),
      mProviderTlsFlags(providerTlsFlags) {}

ClientAuthInfo::ClientAuthInfo(ClientAuthInfo&& aOther) noexcept
    : mHostName(std::move(aOther.mHostName)),
      mOriginAttributes(std::move(aOther.mOriginAttributes)),
      mPort(aOther.mPort),
      mProviderFlags(aOther.mProviderFlags),
      mProviderTlsFlags(aOther.mProviderTlsFlags) {}

const nsACString& ClientAuthInfo::HostName() const { return mHostName; }

const OriginAttributes& ClientAuthInfo::OriginAttributesRef() const {
  return mOriginAttributes;
}

int32_t ClientAuthInfo::Port() const { return mPort; }

uint32_t ClientAuthInfo::ProviderFlags() const { return mProviderFlags; }

uint32_t ClientAuthInfo::ProviderTlsFlags() const { return mProviderTlsFlags; }

class ClientAuthDataRunnable : public SyncRunnableBase {
 public:
  ClientAuthDataRunnable(ClientAuthInfo&& info,
                         const UniqueCERTCertificate& serverCert,
                         nsTArray<nsTArray<uint8_t>>&& collectedCANames)
      : mInfo(std::move(info)),
        mServerCert(serverCert.get()),
        mCollectedCANames(std::move(collectedCANames)),
        mSelectedCertificate(nullptr) {}

  virtual mozilla::pkix::Result BuildChainForCertificate(
      CERTCertificate* cert, UniqueCERTCertList& builtChain);

  // Take the selected certificate. Will be null if none was selected or if an
  // error prevented selecting one.
  UniqueCERTCertificate TakeSelectedCertificate() {
    return std::move(mSelectedCertificate);
  }

 protected:
  virtual void RunOnTargetThread() override;

  ClientAuthInfo mInfo;
  CERTCertificate* const mServerCert;
  nsTArray<nsTArray<uint8_t>> mCollectedCANames;
  nsTArray<nsTArray<uint8_t>> mEnterpriseCertificates;
  UniqueCERTCertificate mSelectedCertificate;
};

class RemoteClientAuthDataRunnable : public ClientAuthDataRunnable {
 public:
  RemoteClientAuthDataRunnable(ClientAuthInfo&& info,
                               const UniqueCERTCertificate& serverCert,
                               nsTArray<nsTArray<uint8_t>>&& collectedCANames)
      : ClientAuthDataRunnable(std::move(info), serverCert,
                               std::move(collectedCANames)) {}

  virtual mozilla::pkix::Result BuildChainForCertificate(
      CERTCertificate* cert, UniqueCERTCertList& builtChain) override;

 protected:
  virtual void RunOnTargetThread() override;

  CopyableTArray<ByteArray> mBuiltChain;
};

nsTArray<nsTArray<uint8_t>> CollectCANames(CERTDistNames* caNames) {
  MOZ_ASSERT(caNames);

  nsTArray<nsTArray<uint8_t>> collectedCANames;
  if (!caNames) {
    return collectedCANames;
  }

  for (int i = 0; i < caNames->nnames; i++) {
    nsTArray<uint8_t> caName;
    caName.AppendElements(caNames->names[i].data, caNames->names[i].len);
    collectedCANames.AppendElement(std::move(caName));
  }
  return collectedCANames;
}

// This callback function is used to pull client certificate
// information upon server request
//
// - arg: SSL data connection
// - socket: SSL socket we're dealing with
// - caNames: list of CA names to use as a hint for selecting potential client
//            certificates (may be empty)
// - pRetCert: returns a pointer to a pointer to a valid certificate if
//             successful; otherwise nullptr
// - pRetKey: returns a pointer to a pointer to the corresponding key if
//            successful; otherwise nullptr
SECStatus nsNSS_SSLGetClientAuthData(void* arg, PRFileDesc* socket,
                                     CERTDistNames* caNames,
                                     CERTCertificate** pRetCert,
                                     SECKEYPrivateKey** pRetKey) {
  if (!socket || !caNames || !pRetCert || !pRetKey) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return SECFailure;
  }

  *pRetCert = nullptr;
  *pRetKey = nullptr;

  RefPtr<nsNSSSocketInfo> info(
      BitwiseCast<nsNSSSocketInfo*, PRFilePrivate*>(socket->higher->secret));

  UniqueCERTCertificate serverCert(SSL_PeerCertificate(socket));
  if (!serverCert) {
    MOZ_ASSERT_UNREACHABLE(
        "Missing server cert should have been detected during server cert "
        "auth.");
    PR_SetError(SSL_ERROR_NO_CERTIFICATE, 0);
    return SECFailure;
  }

  if (info->GetDenyClientCert()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] Not returning client cert due to denyClientCert attribute\n",
             socket));
    return SECSuccess;
  }

  if (info->GetJoined()) {
    // We refuse to send a client certificate when there are multiple hostnames
    // joined on this connection, because we only show the user one hostname
    // (mHostName) in the client certificate UI.

    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] Not returning client cert due to previous join\n", socket));
    return SECSuccess;
  }

  ClientAuthInfo authInfo(info->GetHostName(), info->GetOriginAttributes(),
                          info->GetPort(), info->GetProviderFlags(),
                          info->GetProviderTlsFlags());
  nsTArray<nsTArray<uint8_t>> collectedCANames(CollectCANames(caNames));

  UniqueCERTCertificate selectedCertificate;
  UniqueCERTCertList builtChain;
  SECStatus status = DoGetClientAuthData(std::move(authInfo), serverCert,
                                         std::move(collectedCANames),
                                         selectedCertificate, builtChain);
  if (status != SECSuccess) {
    return status;
  }

  // Currently, the IPC client certs module only refreshes its view of
  // available certificates and keys if the platform issues a search for all
  // certificates or keys. In the socket process, such a search may not have
  // happened, so this ensures it has.
  if (XRE_IsSocketProcess()) {
    UniqueCERTCertList certList(FindClientCertificatesWithPrivateKeys());
    Unused << certList;
  }

  if (selectedCertificate) {
    UniqueSECKEYPrivateKey selectedKey(
        PK11_FindKeyByAnyCert(selectedCertificate.get(), nullptr));
    if (selectedKey) {
      if (builtChain) {
        info->SetClientCertChain(std::move(builtChain));
      } else {
        MOZ_LOG(
            gPIPNSSLog, LogLevel::Debug,
            ("[%p] couldn't determine chain for selected client cert", socket));
      }
      *pRetCert = selectedCertificate.release();
      *pRetKey = selectedKey.release();
      // Make joinConnection prohibit joining after we've sent a client cert
      info->SetSentClientCert();
      if (info->GetSSLVersionUsed() == nsISSLSocketControl::TLS_VERSION_1_3) {
        Telemetry::Accumulate(Telemetry::TLS_1_3_CLIENT_AUTH_USES_PHA,
                              info->IsHandshakeCompleted());
      }
    }
  }

  return SECSuccess;
}

SECStatus DoGetClientAuthData(ClientAuthInfo&& info,
                              const UniqueCERTCertificate& serverCert,
                              nsTArray<nsTArray<uint8_t>>&& collectedCANames,
                              UniqueCERTCertificate& outCert,
                              UniqueCERTCertList& outBuiltChain) {
  // XXX: This should be done asynchronously; see bug 696976
  RefPtr<ClientAuthDataRunnable> runnable =
      XRE_IsSocketProcess()
          ? new RemoteClientAuthDataRunnable(std::move(info), serverCert,
                                             std::move(collectedCANames))
          : new ClientAuthDataRunnable(std::move(info), serverCert,
                                       std::move(collectedCANames));

  nsresult rv = runnable->DispatchToMainThreadAndWait();
  if (NS_FAILED(rv)) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return SECFailure;
  }

  outCert = runnable->TakeSelectedCertificate();
  if (outCert) {
    mozilla::pkix::Result result =
        runnable->BuildChainForCertificate(outCert.get(), outBuiltChain);
    if (result != Success) {
      outBuiltChain.reset(nullptr);
    }
  }

  return SECSuccess;
}

// This TrustDomain only exists to facilitate the mozilla::pkix path building
// algorithm. It considers any certificate with an issuer distinguished name in
// the set of given CA names to be a trust anchor. It does essentially no
// validation or verification (in particular, the signature checking function
// always returns "Success").
class ClientAuthCertNonverifyingTrustDomain final : public TrustDomain {
 public:
  ClientAuthCertNonverifyingTrustDomain(
      nsTArray<nsTArray<uint8_t>>& collectedCANames,
      nsTArray<nsTArray<uint8_t>>& thirdPartyCertificates)
      : mCollectedCANames(collectedCANames),
        mCertStorage(do_GetService(NS_CERT_STORAGE_CID)),
        mThirdPartyCertificates(thirdPartyCertificates) {}

  virtual mozilla::pkix::Result GetCertTrust(
      EndEntityOrCA endEntityOrCA, const CertPolicyId& policy,
      Input candidateCertDER,
      /*out*/ TrustLevel& trustLevel) override;
  virtual mozilla::pkix::Result FindIssuer(Input encodedIssuerName,
                                           IssuerChecker& checker,
                                           Time time) override;

  virtual mozilla::pkix::Result CheckRevocation(
      EndEntityOrCA endEntityOrCA, const CertID& certID, Time time,
      mozilla::pkix::Duration validityDuration,
      /*optional*/ const Input* stapledOCSPresponse,
      /*optional*/ const Input* aiaExtension,
      /*optional*/ const Input* sctExtension) override {
    return Success;
  }

  virtual mozilla::pkix::Result IsChainValid(
      const DERArray& certChain, Time time,
      const CertPolicyId& requiredPolicy) override;

  virtual mozilla::pkix::Result CheckSignatureDigestAlgorithm(
      DigestAlgorithm digestAlg, EndEntityOrCA endEntityOrCA,
      Time notBefore) override {
    return Success;
  }
  virtual mozilla::pkix::Result CheckRSAPublicKeyModulusSizeInBits(
      EndEntityOrCA endEntityOrCA, unsigned int modulusSizeInBits) override {
    return Success;
  }
  virtual mozilla::pkix::Result VerifyRSAPKCS1SignedData(
      Input data, DigestAlgorithm, Input signature,
      Input subjectPublicKeyInfo) override {
    return Success;
  }
  virtual mozilla::pkix::Result VerifyRSAPSSSignedData(
      Input data, DigestAlgorithm, Input signature,
      Input subjectPublicKeyInfo) override {
    return Success;
  }
  virtual mozilla::pkix::Result CheckECDSACurveIsAcceptable(
      EndEntityOrCA endEntityOrCA, NamedCurve curve) override {
    return Success;
  }
  virtual mozilla::pkix::Result VerifyECDSASignedData(
      Input data, DigestAlgorithm, Input signature,
      Input subjectPublicKeyInfo) override {
    return Success;
  }
  virtual mozilla::pkix::Result CheckValidityIsAcceptable(
      Time notBefore, Time notAfter, EndEntityOrCA endEntityOrCA,
      KeyPurposeId keyPurpose) override {
    return Success;
  }
  virtual mozilla::pkix::Result NetscapeStepUpMatchesServerAuth(
      Time notBefore,
      /*out*/ bool& matches) override {
    matches = true;
    return Success;
  }
  virtual void NoteAuxiliaryExtension(AuxiliaryExtension extension,
                                      Input extensionData) override {}
  virtual mozilla::pkix::Result DigestBuf(Input item, DigestAlgorithm digestAlg,
                                          /*out*/ uint8_t* digestBuf,
                                          size_t digestBufLen) override {
    return DigestBufNSS(item, digestAlg, digestBuf, digestBufLen);
  }

  UniqueCERTCertList TakeBuiltChain() { return std::move(mBuiltChain); }

 private:
  nsTArray<nsTArray<uint8_t>>& mCollectedCANames;  // non-owning
  nsCOMPtr<nsICertStorage> mCertStorage;
  nsTArray<nsTArray<uint8_t>>& mThirdPartyCertificates;  // non-owning
  UniqueCERTCertList mBuiltChain;
};

mozilla::pkix::Result ClientAuthCertNonverifyingTrustDomain::GetCertTrust(
    EndEntityOrCA endEntityOrCA, const CertPolicyId& policy,
    Input candidateCertDER,
    /*out*/ TrustLevel& trustLevel) {
  // If the server did not specify any CA names, all client certificates are
  // acceptable.
  if (mCollectedCANames.Length() == 0) {
    trustLevel = TrustLevel::TrustAnchor;
    return Success;
  }
  BackCert cert(candidateCertDER, endEntityOrCA, nullptr);
  mozilla::pkix::Result rv = cert.Init();
  if (rv != Success) {
    return rv;
  }
  // If this certificate's issuer distinguished name is in the set of acceptable
  // CA names, we say this is a trust anchor so that the client certificate
  // issued from this certificate will be presented as an option for the user.
  // We also check the certificate's subject distinguished name to account for
  // the case where client certificates that have the id-kp-OCSPSigning EKU
  // can't be trust anchors according to mozilla::pkix, and thus we may be
  // looking directly at the issuer.
  Input issuer(cert.GetIssuer());
  Input subject(cert.GetSubject());
  for (const auto& caName : mCollectedCANames) {
    Input caNameInput;
    rv = caNameInput.Init(caName.Elements(), caName.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    if (InputsAreEqual(issuer, caNameInput) ||
        InputsAreEqual(subject, caNameInput)) {
      trustLevel = TrustLevel::TrustAnchor;
      return Success;
    }
  }
  trustLevel = TrustLevel::InheritsTrust;
  return Success;
}

// In theory this implementation should only need to consider intermediate
// certificates, since in theory it should only need to look at the issuer
// distinguished name of each certificate to determine if the client
// certificate is considered acceptable to the server.
// However, because we need to account for client certificates with the
// id-kp-OCSPSigning EKU, and because mozilla::pkix doesn't allow such
// certificates to be trust anchors, we need to consider the issuers of such
// certificates directly. These issuers could be roots, so we have to consider
// roots here.
mozilla::pkix::Result ClientAuthCertNonverifyingTrustDomain::FindIssuer(
    Input encodedIssuerName, IssuerChecker& checker, Time time) {
  // First try all relevant certificates known to Gecko, which avoids calling
  // CERT_CreateSubjectCertList, because that can be expensive.
  Vector<Input> geckoCandidates;
  if (!mCertStorage) {
    return mozilla::pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsTArray<uint8_t> subject;
  subject.AppendElements(encodedIssuerName.UnsafeGetData(),
                         encodedIssuerName.GetLength());
  nsTArray<nsTArray<uint8_t>> certs;
  nsresult rv = mCertStorage->FindCertsBySubject(subject, certs);
  if (NS_FAILED(rv)) {
    return mozilla::pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  for (auto& cert : certs) {
    Input certDER;
    mozilla::pkix::Result rv = certDER.Init(cert.Elements(), cert.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    if (!geckoCandidates.append(certDER)) {
      return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
    }
  }

  for (const auto& thirdPartyCertificate : mThirdPartyCertificates) {
    Input thirdPartyCertificateInput;
    mozilla::pkix::Result rv = thirdPartyCertificateInput.Init(
        thirdPartyCertificate.Elements(), thirdPartyCertificate.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    if (!geckoCandidates.append(thirdPartyCertificateInput)) {
      return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
    }
  }

  bool keepGoing = true;
  for (Input candidate : geckoCandidates) {
    mozilla::pkix::Result rv = checker.Check(candidate, nullptr, keepGoing);
    if (rv != Success) {
      return rv;
    }
    if (!keepGoing) {
      return Success;
    }
  }

  SECItem encodedIssuerNameItem = UnsafeMapInputToSECItem(encodedIssuerName);
  // NSS seems not to differentiate between "no potential issuers found" and
  // "there was an error trying to retrieve the potential issuers." We assume
  // there was no error if CERT_CreateSubjectCertList returns nullptr.
  UniqueCERTCertList candidates(CERT_CreateSubjectCertList(
      nullptr, CERT_GetDefaultCertDB(), &encodedIssuerNameItem, 0, false));
  Vector<Input> nssCandidates;
  if (candidates) {
    for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
         !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
      Input certDER;
      mozilla::pkix::Result rv =
          certDER.Init(n->cert->derCert.data, n->cert->derCert.len);
      if (rv != Success) {
        continue;  // probably too big
      }
      if (!nssCandidates.append(certDER)) {
        return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
      }
    }
  }

  for (Input candidate : nssCandidates) {
    mozilla::pkix::Result rv = checker.Check(candidate, nullptr, keepGoing);
    if (rv != Success) {
      return rv;
    }
    if (!keepGoing) {
      return Success;
    }
  }
  return Success;
}

mozilla::pkix::Result ClientAuthCertNonverifyingTrustDomain::IsChainValid(
    const DERArray& certArray, Time, const CertPolicyId&) {
  mBuiltChain = UniqueCERTCertList(CERT_NewCertList());
  if (!mBuiltChain) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  CERTCertDBHandle* certDB(CERT_GetDefaultCertDB());  // non-owning

  size_t numCerts = certArray.GetLength();
  for (size_t i = 0; i < numCerts; ++i) {
    SECItem certDER(UnsafeMapInputToSECItem(*certArray.GetDER(i)));
    UniqueCERTCertificate cert(
        CERT_NewTempCertificate(certDB, &certDER, nullptr, false, true));
    if (!cert) {
      return MapPRErrorCodeToResult(PR_GetError());
    }
    // certArray is ordered with the root first, but we want the resulting
    // certList to have the root last.
    if (CERT_AddCertToListHead(mBuiltChain.get(), cert.get()) != SECSuccess) {
      return MapPRErrorCodeToResult(PR_GetError());
    }
    Unused << cert.release();  // cert is now owned by mBuiltChain.
  }

  return Success;
}

mozilla::pkix::Result ClientAuthDataRunnable::BuildChainForCertificate(
    CERTCertificate* cert, UniqueCERTCertList& builtChain) {
  ClientAuthCertNonverifyingTrustDomain trustDomain(mCollectedCANames,
                                                    mEnterpriseCertificates);
  Input certDER;
  mozilla::pkix::Result result =
      certDER.Init(cert->derCert.data, cert->derCert.len);
  if (result != Success) {
    return result;
  }
  // Client certificates shouldn't be CAs, but for interoperability reasons we
  // attempt to build a path with each certificate as an end entity and then as
  // a CA if that fails.
  const EndEntityOrCA kEndEntityOrCAParams[] = {EndEntityOrCA::MustBeEndEntity,
                                                EndEntityOrCA::MustBeCA};
  // mozilla::pkix rejects certificates with id-kp-OCSPSigning unless it is
  // specifically required. A client certificate should never have this EKU.
  // Unfortunately, there are some client certificates in private PKIs that
  // have this EKU. For interoperability, we attempt to work around this
  // restriction in mozilla::pkix by first building the certificate chain with
  // no particular EKU required and then again with id-kp-OCSPSigning required
  // if that fails.
  const KeyPurposeId kKeyPurposeIdParams[] = {KeyPurposeId::anyExtendedKeyUsage,
                                              KeyPurposeId::id_kp_OCSPSigning};
  for (const auto& endEntityOrCAParam : kEndEntityOrCAParams) {
    for (const auto& keyPurposeIdParam : kKeyPurposeIdParams) {
      mozilla::pkix::Result result =
          BuildCertChain(trustDomain, certDER, Now(), endEntityOrCAParam,
                         KeyUsage::noParticularKeyUsageRequired,
                         keyPurposeIdParam, CertPolicyId::anyPolicy, nullptr);
      if (result == Success) {
        builtChain = trustDomain.TakeBuiltChain();
        return Success;
      }
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("client cert non-validation returned %d for '%s'",
               static_cast<int>(result), cert->subjectName));
    }
  }
  return mozilla::pkix::Result::ERROR_UNKNOWN_ISSUER;
}

void ClientAuthDataRunnable::RunOnTargetThread() {
  // We check the value of a pref in this runnable, so this runnable should only
  // be run on the main thread.
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (NS_WARN_IF(!component)) {
    return;
  }
  nsresult rv = component->GetEnterpriseIntermediates(mEnterpriseCertificates);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  nsTArray<nsTArray<uint8_t>> enterpriseRoots;
  rv = component->GetEnterpriseRoots(enterpriseRoots);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  mEnterpriseCertificates.AppendElements(std::move(enterpriseRoots));

  if (NS_WARN_IF(NS_FAILED(CheckForSmartCardChanges()))) {
    return;
  }

  UniqueCERTCertList certList(FindClientCertificatesWithPrivateKeys());
  if (!certList) {
    return;
  }

  CERTCertListNode* n = CERT_LIST_HEAD(certList);
  while (!CERT_LIST_END(n, certList)) {
    UniqueCERTCertList unusedBuiltChain;
    mozilla::pkix::Result result =
        BuildChainForCertificate(n->cert, unusedBuiltChain);
    if (result != Success) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("removing cert '%s'", n->cert->subjectName));
      CERTCertListNode* toRemove = n;
      n = CERT_LIST_NEXT(n);
      CERT_RemoveCertListNode(toRemove);
      continue;
    }
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("keeping cert '%s'\n", n->cert->subjectName));
    n = CERT_LIST_NEXT(n);
  }

  if (CERT_LIST_EMPTY(certList)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("no client certificates available after filtering by CA"));
    return;
  }

  // find valid user cert and key pair
  if (nsGetUserCertChoice() == UserCertChoice::Auto) {
    // automatically find the right cert
    UniqueCERTCertificate lowPrioNonrepCert;
    // loop through the list until we find a cert with a key
    for (CERTCertListNode* node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList); node = CERT_LIST_NEXT(node)) {
      UniqueSECKEYPrivateKey tmpKey(PK11_FindKeyByAnyCert(node->cert, nullptr));
      if (tmpKey) {
        if (hasExplicitKeyUsageNonRepudiation(node->cert)) {
          // Not a preferred cert
          if (!lowPrioNonrepCert) {  // did not yet find a low prio cert
            lowPrioNonrepCert.reset(CERT_DupCertificate(node->cert));
          }
        } else {
          // this is a good cert to present
          mSelectedCertificate.reset(CERT_DupCertificate(node->cert));
          return;
        }
      }
      if (PR_GetError() == SEC_ERROR_BAD_PASSWORD) {
        // problem with password: bail
        break;
      }
    }

    if (lowPrioNonrepCert) {
      mSelectedCertificate = std::move(lowPrioNonrepCert);
    }
    return;
  }

  // Not Auto => ask
  // Get the SSL Certificate
  const nsACString& hostname = mInfo.HostName();
  nsCOMPtr<nsIClientAuthRememberService> cars = nullptr;

  if (mInfo.ProviderTlsFlags() == 0) {
    cars = do_GetService(NS_CLIENTAUTHREMEMBERSERVICE_CONTRACTID);
  }

  if (cars) {
    nsCString rememberedDBKey;
    bool found;
    nsCOMPtr<nsIX509Cert> cert(new nsNSSCertificate(mServerCert));
    nsresult rv = cars->HasRememberedDecision(
        hostname, mInfo.OriginAttributesRef(), cert, rememberedDBKey, &found);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    if (found) {
      // An empty dbKey indicates that the user chose not to use a certificate
      // and chose to remember this decision
      if (rememberedDBKey.IsEmpty()) {
        return;
      }
      nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
      if (NS_WARN_IF(!certdb)) {
        return;
      }
      nsCOMPtr<nsIX509Cert> foundCert;
      nsresult rv =
          certdb->FindCertByDBKey(rememberedDBKey, getter_AddRefs(foundCert));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }
      if (foundCert) {
        nsNSSCertificate* objCert =
            BitwiseCast<nsNSSCertificate*, nsIX509Cert*>(foundCert.get());
        if (NS_WARN_IF(!objCert)) {
          return;
        }
        mSelectedCertificate.reset(objCert->GetCert());
        if (NS_WARN_IF(!mSelectedCertificate)) {
          return;
        }
        return;
      }
    }
  }

  // ask the user to select a certificate
  nsCOMPtr<nsIClientAuthDialogs> dialogs;
  UniquePORTString corg(CERT_GetOrgName(&mServerCert->subject));
  nsAutoCString org(corg.get());

  UniquePORTString cissuer(CERT_GetOrgName(&mServerCert->issuer));
  nsAutoCString issuer(cissuer.get());

  nsCOMPtr<nsIMutableArray> certArray = nsArrayBase::Create();
  if (NS_WARN_IF(!certArray)) {
    return;
  }

  for (CERTCertListNode* node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList); node = CERT_LIST_NEXT(node)) {
    nsCOMPtr<nsIX509Cert> tempCert = new nsNSSCertificate(node->cert);
    nsresult rv = certArray->AppendElement(tempCert);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  // Throw up the client auth dialog and get back the index of the selected
  // cert
  rv = getNSSDialogs(getter_AddRefs(dialogs), NS_GET_IID(nsIClientAuthDialogs),
                     NS_CLIENTAUTHDIALOGS_CONTRACTID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  uint32_t selectedIndex = 0;
  bool certChosen = false;

  // even if the user has canceled, we want to remember that, to avoid
  // repeating prompts
  bool wantRemember = false;
  rv =
      dialogs->ChooseCertificate(hostname, mInfo.Port(), org, issuer, certArray,
                                 &selectedIndex, &wantRemember, &certChosen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (certChosen) {
    nsCOMPtr<nsIX509Cert> selectedCert =
        do_QueryElementAt(certArray, selectedIndex);
    if (NS_WARN_IF(!selectedCert)) {
      return;
    }
    mSelectedCertificate.reset(selectedCert->GetCert());
    if (NS_WARN_IF(!mSelectedCertificate)) {
      return;
    }
  }

  if (cars && wantRemember) {
    nsCOMPtr<nsIX509Cert> serverCert(new nsNSSCertificate(mServerCert));
    nsCOMPtr<nsIX509Cert> clientCert;
    if (certChosen) {
      clientCert = new nsNSSCertificate(mSelectedCertificate.get());
    }
    rv = cars->RememberDecision(hostname, mInfo.OriginAttributesRef(),
                                serverCert, clientCert);
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }
}

mozilla::pkix::Result RemoteClientAuthDataRunnable::BuildChainForCertificate(
    CERTCertificate*, UniqueCERTCertList& builtChain) {
  builtChain.reset(CERT_NewCertList());
  if (!builtChain) {
    return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
  }

  for (auto& certBytes : mBuiltChain) {
    SECItem certDER = {siBuffer, certBytes.data().Elements(),
                       static_cast<unsigned int>(certBytes.data().Length())};
    UniqueCERTCertificate cert(CERT_NewTempCertificate(
        CERT_GetDefaultCertDB(), &certDER, nullptr, false, true));
    if (!cert) {
      return mozilla::pkix::Result::ERROR_BAD_DER;
    }

    if (CERT_AddCertToListTail(builtChain.get(), cert.get()) != SECSuccess) {
      return mozilla::pkix::Result::FATAL_ERROR_NO_MEMORY;
    }
    Unused << cert.release();
  }

  return Success;
}

void RemoteClientAuthDataRunnable::RunOnTargetThread() {
  MOZ_ASSERT(NS_IsMainThread());

  const ByteArray serverCertSerialized = CopyableTArray<uint8_t>{
      mServerCert->derCert.data, mServerCert->derCert.len};

  nsTArray<ByteArray> collectedCANames;
  for (auto& name : mCollectedCANames) {
    collectedCANames.AppendElement(std::move(name));
  }

  bool succeeded = false;
  ByteArray cert;
  mozilla::net::SocketProcessChild::GetSingleton()->SendGetTLSClientCert(
      nsCString(mInfo.HostName()), mInfo.OriginAttributesRef(), mInfo.Port(),
      mInfo.ProviderFlags(), mInfo.ProviderTlsFlags(), serverCertSerialized,
      collectedCANames, &succeeded, &cert, &mBuiltChain);

  if (!succeeded) {
    return;
  }

  SECItem certItem = {siBuffer, const_cast<uint8_t*>(cert.data().Elements()),
                      static_cast<unsigned int>(cert.data().Length())};
  mSelectedCertificate.reset(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &certItem, nullptr, false, true));
}

static PRFileDesc* nsSSLIOLayerImportFD(PRFileDesc* fd,
                                        nsNSSSocketInfo* infoObject,
                                        const char* host, bool haveHTTPSProxy) {
  PRFileDesc* sslSock = SSL_ImportFD(nullptr, fd);
  if (!sslSock) {
    MOZ_ASSERT_UNREACHABLE("NSS: Error importing socket");
    return nullptr;
  }
  SSL_SetPKCS11PinArg(sslSock, (nsIInterfaceRequestor*)infoObject);
  SSL_HandshakeCallback(sslSock, HandshakeCallback, infoObject);
  SSL_SetCanFalseStartCallback(sslSock, CanFalseStartCallback, infoObject);

  // Disable this hook if we connect anonymously. See bug 466080.
  uint32_t flags = 0;
  infoObject->GetProviderFlags(&flags);
  // Provide the client cert to HTTPS proxy no matter if it is anonymous.
  if (flags & nsISocketProvider::ANONYMOUS_CONNECT && !haveHTTPSProxy &&
      !(flags & nsISocketProvider::ANONYMOUS_CONNECT_ALLOW_CLIENT_CERT)) {
    SSL_GetClientAuthDataHook(sslSock, nullptr, infoObject);
  } else {
    SSL_GetClientAuthDataHook(
        sslSock, (SSLGetClientAuthData)nsNSS_SSLGetClientAuthData, infoObject);
  }

  if (SECSuccess !=
      SSL_AuthCertificateHook(sslSock, AuthCertificateHook, infoObject)) {
    MOZ_ASSERT_UNREACHABLE("Failed to configure AuthCertificateHook");
    goto loser;
  }

  if (SECSuccess != SSL_SetURL(sslSock, host)) {
    MOZ_ASSERT_UNREACHABLE("SSL_SetURL failed");
    goto loser;
  }

  return sslSock;
loser:
  if (sslSock) {
    PR_Close(sslSock);
  }
  return nullptr;
}

// Please change getSignatureName in nsNSSCallbacks.cpp when changing the list
// here. See NOTE at SSL_SignatureSchemePrefSet call site.
static const SSLSignatureScheme sEnabledSignatureSchemes[] = {
    ssl_sig_ecdsa_secp256r1_sha256, ssl_sig_ecdsa_secp384r1_sha384,
    ssl_sig_ecdsa_secp521r1_sha512, ssl_sig_rsa_pss_sha256,
    ssl_sig_rsa_pss_sha384,         ssl_sig_rsa_pss_sha512,
    ssl_sig_rsa_pkcs1_sha256,       ssl_sig_rsa_pkcs1_sha384,
    ssl_sig_rsa_pkcs1_sha512,       ssl_sig_ecdsa_sha1,
    ssl_sig_rsa_pkcs1_sha1,
};

static nsresult nsSSLIOLayerSetOptions(PRFileDesc* fd, bool forSTARTTLS,
                                       bool haveProxy, const char* host,
                                       int32_t port,
                                       nsNSSSocketInfo* infoObject) {
  if (forSTARTTLS || haveProxy) {
    if (SECSuccess != SSL_OptionSet(fd, SSL_SECURITY, false)) {
      return NS_ERROR_FAILURE;
    }
  }

  SSLVersionRange range;
  if (SSL_VersionRangeGet(fd, &range) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // Set TLS 1.3 compat mode.
  if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_TLS13_COMPAT_MODE, PR_TRUE)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error,
            ("[%p] nsSSLIOLayerSetOptions: Setting compat mode failed\n", fd));
  }

  // setting TLS max version
  uint32_t versionFlags =
      getTLSProviderFlagMaxVersion(infoObject->GetProviderTlsFlags());
  if (versionFlags) {
    MOZ_LOG(
        gPIPNSSLog, LogLevel::Debug,
        ("[%p] nsSSLIOLayerSetOptions: version flags %d\n", fd, versionFlags));
    if (versionFlags == kTLSProviderFlagMaxVersion10) {
      range.max = SSL_LIBRARY_VERSION_TLS_1_0;
    } else if (versionFlags == kTLSProviderFlagMaxVersion11) {
      range.max = SSL_LIBRARY_VERSION_TLS_1_1;
    } else if (versionFlags == kTLSProviderFlagMaxVersion12) {
      range.max = SSL_LIBRARY_VERSION_TLS_1_2;
    } else if (versionFlags == kTLSProviderFlagMaxVersion13) {
      range.max = SSL_LIBRARY_VERSION_TLS_1_3;
    } else {
      MOZ_LOG(gPIPNSSLog, LogLevel::Error,
              ("[%p] nsSSLIOLayerSetOptions: unknown version flags %d\n", fd,
               versionFlags));
    }
  }

  if ((infoObject->GetProviderFlags() & nsISocketProvider::BE_CONSERVATIVE) &&
      (range.max > SSL_LIBRARY_VERSION_TLS_1_2)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] nsSSLIOLayerSetOptions: range.max limited to 1.2 due to "
             "BE_CONSERVATIVE flag\n",
             fd));
    range.max = SSL_LIBRARY_VERSION_TLS_1_2;
  }

  uint16_t maxEnabledVersion = range.max;
  infoObject->SharedState().IOLayerHelpers().adjustForTLSIntolerance(
      infoObject->GetHostName(), infoObject->GetPort(), range);
  MOZ_LOG(
      gPIPNSSLog, LogLevel::Debug,
      ("[%p] nsSSLIOLayerSetOptions: using TLS version range (0x%04x,0x%04x)\n",
       fd, static_cast<unsigned int>(range.min),
       static_cast<unsigned int>(range.max)));

  // If the user has set their minimum version to something higher than what
  // we've now set the maximum to, this will result in an inconsistent version
  // range unless we fix it up. This will override their preference, but we only
  // do this for sites critical to the operation of the browser (e.g. update
  // servers) and telemetry experiments.
  if (range.min > range.max) {
    range.min = range.max;
  }

  if (SSL_VersionRangeSet(fd, &range) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  infoObject->SetTLSVersionRange(range);

  // when adjustForTLSIntolerance tweaks the maximum version downward,
  // we tell the server using this SCSV so they can detect a downgrade attack
  if (range.max < maxEnabledVersion) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] nsSSLIOLayerSetOptions: enabling TLS_FALLBACK_SCSV\n", fd));
    // Some servers will choke if we send the fallback SCSV with TLS 1.2.
    if (range.max < SSL_LIBRARY_VERSION_TLS_1_2) {
      if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_FALLBACK_SCSV, true)) {
        return NS_ERROR_FAILURE;
      }
    }
    // tell NSS the max enabled version to make anti-downgrade effective
    if (SECSuccess != SSL_SetDowngradeCheckVersion(fd, maxEnabledVersion)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Enable ECH GREASE if suitable. Has no impact if 'real' ECH is being used.
  if (range.max >= SSL_LIBRARY_VERSION_TLS_1_3 &&
      !(infoObject->GetProviderFlags() & (nsISocketProvider::BE_CONSERVATIVE |
                                          nsISocketTransport::DONT_TRY_ECH)) &&
      StaticPrefs::security_tls_ech_grease_probability()) {
    if ((RandomUint64().valueOr(0) % 100) >=
        100 - StaticPrefs::security_tls_ech_grease_probability()) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
              ("[%p] nsSSLIOLayerSetOptions: enabling TLS ECH Grease\n", fd));
      if (SECSuccess != SSL_EnableTls13GreaseEch(fd, PR_TRUE)) {
        return NS_ERROR_FAILURE;
      }
      // ECH Padding can be between 1 and 255
      if (SECSuccess !=
          SSL_SetTls13GreaseEchSize(
              fd, std::clamp(StaticPrefs::security_tls_ech_grease_size(), 1U,
                             255U))) {
        return NS_ERROR_FAILURE;
      }
    }
    infoObject->UpdateEchExtensionStatus(EchExtensionStatus::kGREASE);
  }

  // Include a modest set of named groups.
  // Please change getKeaGroupName in nsNSSCallbacks.cpp when changing the list
  // here.
  const SSLNamedGroup namedGroups[] = {
      ssl_grp_ec_curve25519, ssl_grp_ec_secp256r1, ssl_grp_ec_secp384r1,
      ssl_grp_ec_secp521r1,  ssl_grp_ffdhe_2048,   ssl_grp_ffdhe_3072};
  if (SECSuccess != SSL_NamedGroupConfig(fd, namedGroups,
                                         mozilla::ArrayLength(namedGroups))) {
    return NS_ERROR_FAILURE;
  }
  // This ensures that we send key shares for X25519 and P-256 in TLS 1.3, so
  // that servers are less likely to use HelloRetryRequest.
  if (SECSuccess != SSL_SendAdditionalKeyShares(fd, 1)) {
    return NS_ERROR_FAILURE;
  }

  // NOTE: Should this list ever include ssl_sig_rsa_pss_pss_sha* (or should
  // it become possible to enable this scheme via a pref), it is required
  // to test that a Delegated Credential containing a small-modulus RSA-PSS SPKI
  // is properly rejected. NSS will not advertise PKCS1 or RSAE schemes (which
  // the |ssl_sig_rsa_pss_*| defines alias, meaning we will not currently accept
  // any RSA DC.
  if (SECSuccess != SSL_SignatureSchemePrefSet(
                        fd, sEnabledSignatureSchemes,
                        mozilla::ArrayLength(sEnabledSignatureSchemes))) {
    return NS_ERROR_FAILURE;
  }

  bool enabled = infoObject->SharedState().IsOCSPStaplingEnabled();
  if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_OCSP_STAPLING, enabled)) {
    return NS_ERROR_FAILURE;
  }

  bool sctsEnabled = infoObject->SharedState().IsSignedCertTimestampsEnabled();
  if (SECSuccess !=
      SSL_OptionSet(fd, SSL_ENABLE_SIGNED_CERT_TIMESTAMPS, sctsEnabled)) {
    return NS_ERROR_FAILURE;
  }

  if (SECSuccess != SSL_OptionSet(fd, SSL_HANDSHAKE_AS_CLIENT, true)) {
    return NS_ERROR_FAILURE;
  }

#if defined(__arm__) && not defined(__ARM_FEATURE_CRYPTO)
  unsigned int enabledCiphers = 0;
  std::vector<uint16_t> ciphers(SSL_GetNumImplementedCiphers());

  // Returns only the enabled (reflecting prefs) ciphers, ordered
  // by their occurence in
  // https://hg.mozilla.org/projects/nss/file/a75ea4cdacd95282c6c245ebb849c25e84ccd908/lib/ssl/ssl3con.c#l87
  if (SSL_CipherSuiteOrderGet(fd, ciphers.data(), &enabledCiphers) !=
      SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // On ARM, prefer (TLS_CHACHA20_POLY1305_SHA256) over AES when hardware
  // support for AES isn't available. However, it may be disabled. If enabled,
  // it will either be element [0] or [1]*. If [0], we're done. If [1], swap it
  // with [0] (TLS_AES_128_GCM_SHA256).
  // *(assuming the compile-time order remains unchanged)
  if (enabledCiphers > 1) {
    if (ciphers[0] != TLS_CHACHA20_POLY1305_SHA256 &&
        ciphers[1] == TLS_CHACHA20_POLY1305_SHA256) {
      std::swap(ciphers[0], ciphers[1]);

      if (SSL_CipherSuiteOrderSet(fd, ciphers.data(), enabledCiphers) !=
          SECSuccess) {
        return NS_ERROR_FAILURE;
      }
    }
  }
#endif

  // Set the Peer ID so that SSL proxy connections work properly and to
  // separate anonymous and/or private browsing connections.
  nsAutoCString peerId;
  infoObject->GetPeerId(peerId);
  if (SECSuccess != SSL_SetSockPeerID(fd, peerId.get())) {
    return NS_ERROR_FAILURE;
  }

  uint32_t flags = infoObject->GetProviderFlags();
  if (flags & nsISocketProvider::NO_PERMANENT_STORAGE) {
    if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_SESSION_TICKETS, false) ||
        SECSuccess != SSL_OptionSet(fd, SSL_NO_CACHE, true)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

SECStatus StoreResumptionToken(PRFileDesc* fd, const PRUint8* resumptionToken,
                               unsigned int len, void* ctx) {
  PRIntn val;
  if (SSL_OptionGet(fd, SSL_ENABLE_SESSION_TICKETS, &val) != SECSuccess ||
      val == 0) {
    return SECFailure;
  }

  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*)ctx;
  if (!infoObject) {
    return SECFailure;
  }

  nsAutoCString peerId;
  infoObject->GetPeerId(peerId);
  if (NS_FAILED(
          net::SSLTokensCache::Put(peerId, resumptionToken, len, infoObject))) {
    return SECFailure;
  }

  return SECSuccess;
}

nsresult nsSSLIOLayerAddToSocket(int32_t family, const char* host, int32_t port,
                                 nsIProxyInfo* proxy,
                                 const OriginAttributes& originAttributes,
                                 PRFileDesc* fd, nsISupports** info,
                                 bool forSTARTTLS, uint32_t providerFlags,
                                 uint32_t providerTlsFlags) {
  PRFileDesc* layer = nullptr;
  PRFileDesc* plaintextLayer = nullptr;
  nsresult rv;
  PRStatus stat;

  SharedSSLState* sharedState = nullptr;
  RefPtr<SharedSSLState> allocatedState;
  if (providerTlsFlags) {
    allocatedState = new SharedSSLState(providerTlsFlags);
    sharedState = allocatedState.get();
  } else {
    bool isPrivate = providerFlags & nsISocketProvider::NO_PERMANENT_STORAGE ||
                     originAttributes.mPrivateBrowsingId !=
                         OriginAttributes().mPrivateBrowsingId;
    sharedState = isPrivate ? PrivateSSLState() : PublicSSLState();
  }

  nsNSSSocketInfo* infoObject =
      new nsNSSSocketInfo(*sharedState, providerFlags, providerTlsFlags);
  if (!infoObject) return NS_ERROR_FAILURE;

  NS_ADDREF(infoObject);
  infoObject->SetForSTARTTLS(forSTARTTLS);
  infoObject->SetHostName(host);
  infoObject->SetPort(port);
  infoObject->SetOriginAttributes(originAttributes);
  if (allocatedState) {
    infoObject->SetSharedOwningReference(allocatedState);
  }

  bool haveProxy = false;
  bool haveHTTPSProxy = false;
  if (proxy) {
    nsAutoCString proxyHost;
    proxy->GetHost(proxyHost);
    haveProxy = !proxyHost.IsEmpty();
    nsAutoCString type;
    haveHTTPSProxy = haveProxy && NS_SUCCEEDED(proxy->GetType(type)) &&
                     type.EqualsLiteral("https");
  }

  // A plaintext observer shim is inserted so we can observe some protocol
  // details without modifying nss
  plaintextLayer =
      PR_CreateIOLayerStub(nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity,
                           &nsSSLIOLayerHelpers::nsSSLPlaintextLayerMethods);
  if (plaintextLayer) {
    plaintextLayer->secret = (PRFilePrivate*)infoObject;
    stat = PR_PushIOLayer(fd, PR_TOP_IO_LAYER, plaintextLayer);
    if (stat == PR_FAILURE) {
      plaintextLayer->dtor(plaintextLayer);
      plaintextLayer = nullptr;
    }
  }

  PRFileDesc* sslSock =
      nsSSLIOLayerImportFD(fd, infoObject, host, haveHTTPSProxy);
  if (!sslSock) {
    MOZ_ASSERT_UNREACHABLE("NSS: Error importing socket");
    goto loser;
  }

  infoObject->SetFileDescPtr(sslSock);

  rv = nsSSLIOLayerSetOptions(sslSock, forSTARTTLS, haveProxy, host, port,
                              infoObject);

  if (NS_FAILED(rv)) goto loser;

  // Now, layer ourselves on top of the SSL socket...
  layer = PR_CreateIOLayerStub(nsSSLIOLayerHelpers::nsSSLIOLayerIdentity,
                               &nsSSLIOLayerHelpers::nsSSLIOLayerMethods);
  if (!layer) goto loser;

  layer->secret = (PRFilePrivate*)infoObject;
  stat = PR_PushIOLayer(sslSock, PR_GetLayersIdentity(sslSock), layer);

  if (stat == PR_FAILURE) {
    goto loser;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] Socket set up\n", (void*)sslSock));
  infoObject->QueryInterface(NS_GET_IID(nsISupports), (void**)(info));

  // We are going use a clear connection first //
  if (forSTARTTLS || haveProxy) {
    infoObject->SetHandshakeNotPending();
  }

  infoObject->SharedState().NoteSocketCreated();

  if (StaticPrefs::network_ssl_tokens_cache_enabled()) {
    rv = infoObject->SetResumptionTokenFromExternalCache();
    if (NS_FAILED(rv)) {
      return rv;
    }
    SSL_SetResumptionTokenCallback(sslSock, &StoreResumptionToken, infoObject);
  }

  return NS_OK;
loser:
  NS_IF_RELEASE(infoObject);
  if (layer) {
    layer->dtor(layer);
  }
  if (plaintextLayer) {
    // Note that PR_*IOLayer operations may modify the stack of fds, so a
    // previously-valid pointer may no longer point to what we think it points
    // to after calling PR_PopIOLayer. We must operate on the pointer returned
    // by PR_PopIOLayer.
    plaintextLayer =
        PR_PopIOLayer(fd, nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity);
    plaintextLayer->dtor(plaintextLayer);
  }
  return NS_ERROR_FAILURE;
}

already_AddRefed<IPCClientCertsChild> GetIPCClientCertsActor() {
  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForSocketParentBridgeForCurrentThread();
  if (!backgroundActor) {
    return nullptr;
  }
  RefPtr<PIPCClientCertsChild> actor =
      SingleManagedOrNull(backgroundActor->ManagedPIPCClientCertsChild());
  if (!actor) {
    actor = backgroundActor->SendPIPCClientCertsConstructor(
        new IPCClientCertsChild());
    if (!actor) {
      return nullptr;
    }
  }
  return actor.forget().downcast<IPCClientCertsChild>();
}

extern "C" {

const uint8_t kIPCClientCertsObjectTypeCert = 1;
const uint8_t kIPCClientCertsObjectTypeRSAKey = 2;
const uint8_t kIPCClientCertsObjectTypeECKey = 3;

// This function is provided to the IPC client certs module so it can cause the
// parent process to find certificates and keys and send identifying
// information about them over IPC.
void DoFindObjects(FindObjectsCallback cb, void* ctx) {
  RefPtr<IPCClientCertsChild> ipcClientCertsActor(GetIPCClientCertsActor());
  if (!ipcClientCertsActor) {
    return;
  }
  nsTArray<IPCClientCertObject> objects;
  if (!ipcClientCertsActor->SendFindObjects(&objects)) {
    return;
  }
  for (const auto& object : objects) {
    switch (object.type()) {
      case IPCClientCertObject::TECKey:
        cb(kIPCClientCertsObjectTypeECKey, object.get_ECKey().params().Length(),
           object.get_ECKey().params().Elements(),
           object.get_ECKey().cert().Length(),
           object.get_ECKey().cert().Elements(), object.get_ECKey().slotType(),
           ctx);
        break;
      case IPCClientCertObject::TRSAKey:
        cb(kIPCClientCertsObjectTypeRSAKey,
           object.get_RSAKey().modulus().Length(),
           object.get_RSAKey().modulus().Elements(),
           object.get_RSAKey().cert().Length(),
           object.get_RSAKey().cert().Elements(),
           object.get_RSAKey().slotType(), ctx);
        break;
      case IPCClientCertObject::TCertificate:
        cb(kIPCClientCertsObjectTypeCert,
           object.get_Certificate().der().Length(),
           object.get_Certificate().der().Elements(), 0, nullptr,
           object.get_Certificate().slotType(), ctx);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unhandled IPCClientCertObject type");
        break;
    }
  }
}

// This function is provided to the IPC client certs module so it can cause the
// parent process to sign the given data using the key corresponding to the
// given certificate, using the given parameters.
void DoSign(size_t cert_len, const uint8_t* cert, size_t data_len,
            const uint8_t* data, size_t params_len, const uint8_t* params,
            SignCallback cb, void* ctx) {
  RefPtr<IPCClientCertsChild> ipcClientCertsActor(GetIPCClientCertsActor());
  if (!ipcClientCertsActor) {
    return;
  }
  ByteArray certBytes(nsTArray<uint8_t>(cert, cert_len));
  ByteArray dataBytes(nsTArray<uint8_t>(data, data_len));
  ByteArray paramsBytes(nsTArray<uint8_t>(params, params_len));
  ByteArray signature;
  if (!ipcClientCertsActor->SendSign(certBytes, dataBytes, paramsBytes,
                                     &signature)) {
    return;
  }
  cb(signature.data().Length(), signature.data().Elements(), ctx);
}
}  // extern "C"

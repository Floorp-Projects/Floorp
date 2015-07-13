/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSIOLayer.h"

#include "pkix/pkixtypes.h"
#include "nsNSSComponent.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/Casting.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Telemetry.h"

#include "mozilla/Logging.h"
#include "prmem.h"
#include "prnetdb.h"
#include "nsIPrefService.h"
#include "nsIClientAuthDialogs.h"
#include "nsIWebProgressListener.h"
#include "nsClientAuthRemember.h"
#include "nsServiceManagerUtils.h"

#include "nsISocketProvider.h"
#include "nsPrintfCString.h"
#include "SSLServerCertVerification.h"
#include "nsNSSCertHelper.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsIConsoleService.h"
#include "PSMRunnable.h"
#include "ScopedNSSTypes.h"
#include "SharedSSLState.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "NSSCertDBTrustDomain.h"
#include "NSSErrorsService.h"

#include "ssl.h"
#include "sslproto.h"
#include "secerr.h"
#include "sslerr.h"
#include "secder.h"
#include "keyhi.h"

#include <algorithm>

#include "IntolerantFallbackList.inc"

using namespace mozilla;
using namespace mozilla::psm;

//#define DEBUG_SSL_VERBOSE //Enable this define to get minimal
                            //reports when doing SSL read/write

//#define DUMP_BUFFER  //Enable this define along with
                       //DEBUG_SSL_VERBOSE to dump SSL
                       //read/write buffer to a log.
                       //Uses PR_LOG except on Mac where
                       //we always write out to our own
                       //file.

namespace {

void
getSiteKey(const nsACString& hostName, uint16_t port,
           /*out*/ nsCSubstring& key)
{
  key = hostName;
  key.AppendASCII(":");
  key.AppendInt(port);
}

// SSM_UserCertChoice: enum for cert choice info
typedef enum {ASK, AUTO} SSM_UserCertChoice;

// Historically, we have required that the server negotiate ALPN or NPN in
// order to false start, as a compatibility hack to work around
// implementations that just stop responding during false start. However, now
// false start is resricted to modern crypto (TLS 1.2 and AEAD cipher suites)
// so it is less likely that requring NPN or ALPN is still necessary.
static const bool FALSE_START_REQUIRE_NPN_DEFAULT = false;

} // unnamed namespace

extern PRLogModuleInfo* gPIPNSSLog;

nsNSSSocketInfo::nsNSSSocketInfo(SharedSSLState& aState, uint32_t providerFlags)
  : mFd(nullptr),
    mCertVerificationState(before_cert_verification),
    mSharedState(aState),
    mForSTARTTLS(false),
    mHandshakePending(true),
    mRememberClientAuthCertificate(false),
    mPreliminaryHandshakeDone(false),
    mNPNCompleted(false),
    mFalseStartCallbackCalled(false),
    mFalseStarted(false),
    mIsFullHandshake(false),
    mHandshakeCompleted(false),
    mJoined(false),
    mSentClientCert(false),
    mNotedTimeUntilReady(false),
    mFailedVerification(false),
    mKEAUsed(nsISSLSocketControl::KEY_EXCHANGE_UNKNOWN),
    mKEAKeyBits(0),
    mSSLVersionUsed(nsISSLSocketControl::SSL_VERSION_UNKNOWN),
    mMACAlgorithmUsed(nsISSLSocketControl::SSL_MAC_UNKNOWN),
    mBypassAuthentication(false),
    mProviderFlags(providerFlags),
    mSocketCreationTimestamp(TimeStamp::Now()),
    mPlaintextBytesRead(0),
    mClientCert(nullptr)
{
  mTLSVersionRange.min = 0;
  mTLSVersionRange.max = 0;
}

nsNSSSocketInfo::~nsNSSSocketInfo()
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsNSSSocketInfo, TransportSecurityInfo,
                            nsISSLSocketControl,
                            nsIClientAuthUserDecision)

NS_IMETHODIMP
nsNSSSocketInfo::GetProviderFlags(uint32_t* aProviderFlags)
{
  *aProviderFlags = mProviderFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetKEAUsed(int16_t* aKea)
{
  *aKea = mKEAUsed;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetKEAKeyBits(uint32_t* aKeyBits)
{
  *aKeyBits = mKEAKeyBits;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetSSLVersionUsed(int16_t* aSSLVersionUsed)
{
  *aSSLVersionUsed = mSSLVersionUsed;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetSSLVersionOffered(int16_t* aSSLVersionOffered)
{
  *aSSLVersionOffered = mTLSVersionRange.max;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetMACAlgorithmUsed(int16_t* aMac)
{
  *aMac = mMACAlgorithmUsed;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetClientCert(nsIX509Cert** aClientCert)
{
  NS_ENSURE_ARG_POINTER(aClientCert);
  *aClientCert = mClientCert;
  NS_IF_ADDREF(*aClientCert);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetClientCert(nsIX509Cert* aClientCert)
{
  mClientCert = aClientCert;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetBypassAuthentication(bool* arg)
{
  *arg = mBypassAuthentication;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetFailedVerification(bool* arg)
{
  *arg = mFailedVerification;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetRememberClientAuthCertificate(bool* aRemember)
{
  NS_ENSURE_ARG_POINTER(aRemember);
  *aRemember = mRememberClientAuthCertificate;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetRememberClientAuthCertificate(bool aRemember)
{
  mRememberClientAuthCertificate = aRemember;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks)
{
  *aCallbacks = mCallbacks;
  NS_IF_ADDREF(*aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks)
{
  if (!aCallbacks) {
    mCallbacks = nullptr;
    return NS_OK;
  }

  mCallbacks = aCallbacks;

  return NS_OK;
}

void
nsNSSSocketInfo::NoteTimeUntilReady()
{
  if (mNotedTimeUntilReady)
    return;

  mNotedTimeUntilReady = true;

  // This will include TCP and proxy tunnel wait time
  Telemetry::AccumulateTimeDelta(Telemetry::SSL_TIME_UNTIL_READY,
                                 mSocketCreationTimestamp, TimeStamp::Now());
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
         ("[%p] nsNSSSocketInfo::NoteTimeUntilReady\n", mFd));
}

void
nsNSSSocketInfo::SetHandshakeCompleted()
{
  if (!mHandshakeCompleted) {
    enum HandshakeType {
      Resumption = 1,
      FalseStarted = 2,
      ChoseNotToFalseStart = 3,
      NotAllowedToFalseStart = 4,
    };

    HandshakeType handshakeType = !IsFullHandshake() ? Resumption
                                : mFalseStarted ? FalseStarted
                                : mFalseStartCallbackCalled ? ChoseNotToFalseStart
                                : NotAllowedToFalseStart;

    // This will include TCP and proxy tunnel wait time
    Telemetry::AccumulateTimeDelta(Telemetry::SSL_TIME_UNTIL_HANDSHAKE_FINISHED,
                                   mSocketCreationTimestamp, TimeStamp::Now());

    // If the handshake is completed for the first time from just 1 callback
    // that means that TLS session resumption must have been used.
    Telemetry::Accumulate(Telemetry::SSL_RESUMED_SESSION,
                          handshakeType == Resumption);
    Telemetry::Accumulate(Telemetry::SSL_HANDSHAKE_TYPE, handshakeType);
  }


    // Remove the plain text layer as it is not needed anymore.
    // The plain text layer is not always present - so its not a fatal error
    // if it cannot be removed
    PRFileDesc* poppedPlaintext =
      PR_GetIdentitiesLayer(mFd, nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity);
    if (poppedPlaintext) {
      PR_PopIOLayer(mFd, nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity);
      poppedPlaintext->dtor(poppedPlaintext);
    }

    mHandshakeCompleted = true;

    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("[%p] nsNSSSocketInfo::SetHandshakeCompleted\n", (void*) mFd));

    mIsFullHandshake = false; // reset for next handshake on this connection
}

void
nsNSSSocketInfo::SetNegotiatedNPN(const char* value, uint32_t length)
{
  if (!value) {
    mNegotiatedNPN.Truncate();
  } else {
    mNegotiatedNPN.Assign(value, length);
  }
  mNPNCompleted = true;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetNegotiatedNPN(nsACString& aNegotiatedNPN)
{
  if (!mNPNCompleted)
    return NS_ERROR_NOT_CONNECTED;

  aNegotiatedNPN = mNegotiatedNPN;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::IsAcceptableForHost(const nsACString& hostname, bool* _retval)
{
  // If this is the same hostname then the certicate status does not
  // need to be considered. They are joinable.
  if (hostname.Equals(GetHostName())) {
    *_retval = true;
    return NS_OK;
  }

  // Before checking the server certificate we need to make sure the
  // handshake has completed.
  if (!mHandshakeCompleted || !SSLStatus() || !SSLStatus()->HasServerCert()) {
    return NS_OK;
  }

  // If the cert has error bits (e.g. it is untrusted) then do not join.
  // The value of mHaveCertErrorBits is only reliable because we know that
  // the handshake completed.
  if (SSLStatus()->mHaveCertErrorBits)
    return NS_OK;

  // If the connection is using client certificates then do not join
  // because the user decides on whether to send client certs to hosts on a
  // per-domain basis.
  if (mSentClientCert)
    return NS_OK;

  // Ensure that the server certificate covers the hostname that would
  // like to join this connection

  ScopedCERTCertificate nssCert;

  nsCOMPtr<nsIX509Cert> cert;
  if (NS_FAILED(SSLStatus()->GetServerCert(getter_AddRefs(cert)))) {
    return NS_OK;
  }
  if (cert) {
    nssCert = cert->GetCert();
  }

  if (!nssCert) {
    return NS_OK;
  }

  // Attempt to verify the joinee's certificate using the joining hostname.
  // This ensures that any hostname-specific verification logic (e.g. key
  // pinning) is satisfied by the joinee's certificate chain.
  // This verification only uses local information; since we're on the network
  // thread, we would be blocking on ourselves if we attempted any network i/o.
  // TODO(bug 1056935): The certificate chain built by this verification may be
  // different than the certificate chain originally built during the joined
  // connection's TLS handshake. Consequently, we may report a wrong and/or
  // misleading certificate chain for HTTP transactions coalesced onto this
  // connection. This may become problematic in the future. For example,
  // if/when we begin relying on intermediate certificates being stored in the
  // securityInfo of a cached HTTPS response, that cached certificate chain may
  // actually be the wrong chain. We should consider having JoinConnection
  // return the certificate chain built here, so that the calling Necko code
  // can associate the correct certificate chain with the HTTP transactions it
  // is trying to join onto this connection.
  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  if (!certVerifier) {
    return NS_OK;
  }
  nsAutoCString hostnameFlat(PromiseFlatCString(hostname));
  CertVerifier::Flags flags = CertVerifier::FLAG_LOCAL_ONLY;
  SECStatus rv = certVerifier->VerifySSLServerCert(nssCert, nullptr,
                                                   mozilla::pkix::Now(),
                                                   nullptr, hostnameFlat.get(),
                                                   false, flags, nullptr,
                                                   nullptr);
  if (rv != SECSuccess) {
    return NS_OK;
  }

  // All tests pass
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::JoinConnection(const nsACString& npnProtocol,
                                const nsACString& hostname,
                                int32_t port,
                                bool* _retval)
{
  *_retval = false;

  // Different ports may not be joined together
  if (port != GetPort())
    return NS_OK;

  // Make sure NPN has been completed and matches requested npnProtocol
  if (!mNPNCompleted || !mNegotiatedNPN.Equals(npnProtocol))
    return NS_OK;

  if (mBypassAuthentication) {
    // An unauthenticated connection does not know whether or not it
    // is acceptable for a particular hostname
    return NS_OK;
  }

  IsAcceptableForHost(hostname, _retval);

  if (*_retval) {
    // All tests pass - this is joinable
    mJoined = true;
  }
  return NS_OK;
}

bool
nsNSSSocketInfo::GetForSTARTTLS()
{
  return mForSTARTTLS;
}

void
nsNSSSocketInfo::SetForSTARTTLS(bool aForSTARTTLS)
{
  mForSTARTTLS = aForSTARTTLS;
}

NS_IMETHODIMP
nsNSSSocketInfo::ProxyStartSSL()
{
  return ActivateSSL();
}

NS_IMETHODIMP
nsNSSSocketInfo::StartTLS()
{
  return ActivateSSL();
}

NS_IMETHODIMP
nsNSSSocketInfo::SetNPNList(nsTArray<nsCString>& protocolArray)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;
  if (!mFd)
    return NS_ERROR_FAILURE;

  // the npn list is a concatenated list of 8 bit byte strings.
  nsCString npnList;

  for (uint32_t index = 0; index < protocolArray.Length(); ++index) {
    if (protocolArray[index].IsEmpty() ||
        protocolArray[index].Length() > 255)
      return NS_ERROR_ILLEGAL_VALUE;

    npnList.Append(protocolArray[index].Length());
    npnList.Append(protocolArray[index]);
  }

  if (SSL_SetNextProtoNego(
        mFd,
        reinterpret_cast<const unsigned char*>(npnList.get()),
        npnList.Length()) != SECSuccess)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsNSSSocketInfo::ActivateSSL()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  if (SECSuccess != SSL_OptionSet(mFd, SSL_SECURITY, true))
    return NS_ERROR_FAILURE;
  if (SECSuccess != SSL_ResetHandshake(mFd, false))
    return NS_ERROR_FAILURE;

  mHandshakePending = true;

  return NS_OK;
}

nsresult
nsNSSSocketInfo::GetFileDescPtr(PRFileDesc** aFilePtr)
{
  *aFilePtr = mFd;
  return NS_OK;
}

nsresult
nsNSSSocketInfo::SetFileDescPtr(PRFileDesc* aFilePtr)
{
  mFd = aFilePtr;
  return NS_OK;
}

void
nsNSSSocketInfo::SetCertVerificationWaiting()
{
  // mCertVerificationState may be before_cert_verification for the first
  // handshake on the connection, or after_cert_verification for subsequent
  // renegotiation handshakes.
  NS_ASSERTION(mCertVerificationState != waiting_for_cert_verification,
               "Invalid state transition to waiting_for_cert_verification");
  mCertVerificationState = waiting_for_cert_verification;
}

// Be careful that SetCertVerificationResult does NOT get called while we are
// processing a SSL callback function, because SSL_AuthCertificateComplete will
// attempt to acquire locks that are already held by libssl when it calls
// callbacks.
void
nsNSSSocketInfo::SetCertVerificationResult(PRErrorCode errorCode,
                                           SSLErrorMessageType errorMessageType)
{
  NS_ASSERTION(mCertVerificationState == waiting_for_cert_verification,
               "Invalid state transition to cert_verification_finished");

  if (mFd) {
    SECStatus rv = SSL_AuthCertificateComplete(mFd, errorCode);
    // Only replace errorCode if there was originally no error
    if (rv != SECSuccess && errorCode == 0) {
      errorCode = PR_GetError();
      errorMessageType = PlainErrorMessage;
      if (errorCode == 0) {
        NS_ERROR("SSL_AuthCertificateComplete didn't set error code");
        errorCode = PR_INVALID_STATE_ERROR;
      }
    }
  }

  if (errorCode) {
    mFailedVerification = true;
    SetCanceled(errorCode, errorMessageType);
  }

  if (mPlaintextBytesRead && !errorCode) {
    Telemetry::Accumulate(Telemetry::SSL_BYTES_BEFORE_CERT_CALLBACK,
                          AssertedCast<uint32_t>(mPlaintextBytesRead));
  }

  mCertVerificationState = after_cert_verification;
}

SharedSSLState&
nsNSSSocketInfo::SharedState()
{
  return mSharedState;
}

void nsSSLIOLayerHelpers::Cleanup()
{
  MutexAutoLock lock(mutex);
  mTLSIntoleranceInfo.Clear();
  mInsecureFallbackSites.Clear();
}

static void
nsHandleSSLError(nsNSSSocketInfo* socketInfo,
                 ::mozilla::psm::SSLErrorMessageType errtype,
                 PRErrorCode err)
{
  if (!NS_IsMainThread()) {
    NS_ERROR("nsHandleSSLError called off the main thread");
    return;
  }

  // SetCanceled is only called by the main thread or the socket transport
  // thread. Whenever this function is called on the main thread, the SSL
  // thread is blocked on it. So, no mutex is necessary for
  // SetCanceled()/GetError*().
  if (socketInfo->GetErrorCode()) {
    // If the socket has been flagged as canceled,
    // the code who did was responsible for setting the error code.
    return;
  }

  // We must cancel first, which sets the error code.
  socketInfo->SetCanceled(err, PlainErrorMessage);
  nsXPIDLString errorString;
  socketInfo->GetErrorLogMessage(err, errtype, errorString);

  if (!errorString.IsEmpty()) {
    nsContentUtils::LogSimpleConsoleError(errorString, "SSL");
  }
}

namespace {

enum Operation { reading, writing, not_reading_or_writing };

int32_t checkHandshake(int32_t bytesTransfered, bool wasReading,
                       PRFileDesc* ssl_layer_fd,
                       nsNSSSocketInfo* socketInfo);

nsNSSSocketInfo*
getSocketInfoIfRunning(PRFileDesc* fd, Operation op,
                       const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  if (!fd || !fd->lower || !fd->secret ||
      fd->identity != nsSSLIOLayerHelpers::nsSSLIOLayerIdentity) {
    NS_ERROR("bad file descriptor passed to getSocketInfoIfRunning");
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return nullptr;
  }

  nsNSSSocketInfo* socketInfo = (nsNSSSocketInfo*) fd->secret;

  if (socketInfo->isAlreadyShutDown() || socketInfo->isPK11LoggedOut()) {
    PR_SetError(PR_SOCKET_SHUTDOWN_ERROR, 0);
    return nullptr;
  }

  if (socketInfo->GetErrorCode()) {
    PRErrorCode err = socketInfo->GetErrorCode();
    PR_SetError(err, 0);
    if (op == reading || op == writing) {
      // We must do TLS intolerance checks for reads and writes, for timeouts
      // in particular.
      (void) checkHandshake(-1, op == reading, fd, socketInfo);
    }

    // If we get here, it is probably because cert verification failed and this
    // is the first I/O attempt since that failure.
    return nullptr;
  }

  return socketInfo;
}

} // namespace

static PRStatus
nsSSLIOLayerConnect(PRFileDesc* fd, const PRNetAddr* addr,
                    PRIntervalTime timeout)
{
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] connecting SSL socket\n",
         (void*) fd));
  nsNSSShutDownPreventionLock locker;
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing, locker))
    return PR_FAILURE;

  PRStatus status = fd->lower->methods->connect(fd->lower, addr, timeout);
  if (status != PR_SUCCESS) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Error, ("[%p] Lower layer connect error: %d\n",
                                      (void*) fd, PR_GetError()));
    return status;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] Connect\n", (void*) fd));
  return status;
}

void
nsSSLIOLayerHelpers::rememberTolerantAtVersion(const nsACString& hostName,
                                               int16_t port, uint16_t tolerant)
{
  nsCString key;
  getSiteKey(hostName, port, key);

  MutexAutoLock lock(mutex);

  IntoleranceEntry entry;
  if (mTLSIntoleranceInfo.Get(key, &entry)) {
    entry.AssertInvariant();
    entry.tolerant = std::max(entry.tolerant, tolerant);
    if (entry.intolerant != 0 && entry.intolerant <= entry.tolerant) {
      entry.intolerant = entry.tolerant + 1;
      entry.intoleranceReason = 0; // lose the reason
    }
    if (entry.strongCipherStatus == StrongCipherStatusUnknown) {
      entry.strongCipherStatus = StrongCiphersWorked;
    }
  } else {
    entry.tolerant = tolerant;
    entry.intolerant = 0;
    entry.intoleranceReason = 0;
    entry.strongCipherStatus = StrongCiphersWorked;
  }

  entry.AssertInvariant();

  mTLSIntoleranceInfo.Put(key, entry);
}

uint16_t
nsSSLIOLayerHelpers::forgetIntolerance(const nsACString& hostName,
                                       int16_t port)
{
  nsCString key;
  getSiteKey(hostName, port, key);

  MutexAutoLock lock(mutex);

  uint16_t tolerant = 0;
  IntoleranceEntry entry;
  if (mTLSIntoleranceInfo.Get(key, &entry)) {
    entry.AssertInvariant();

    tolerant = entry.tolerant;
    entry.intolerant = 0;
    entry.intoleranceReason = 0;
    if (entry.strongCipherStatus != StrongCiphersWorked) {
      entry.strongCipherStatus = StrongCipherStatusUnknown;
    }

    entry.AssertInvariant();
    mTLSIntoleranceInfo.Put(key, entry);
  }

  return tolerant;
}

bool
nsSSLIOLayerHelpers::fallbackLimitReached(const nsACString& hostName,
                                          uint16_t intolerant)
{
  if (isInsecureFallbackSite(hostName)) {
    return intolerant <= SSL_LIBRARY_VERSION_TLS_1_0;
  }
  return intolerant <= mVersionFallbackLimit;
}

// returns true if we should retry the handshake
bool
nsSSLIOLayerHelpers::rememberIntolerantAtVersion(const nsACString& hostName,
                                                 int16_t port,
                                                 uint16_t minVersion,
                                                 uint16_t intolerant,
                                                 PRErrorCode intoleranceReason)
{
  if (intolerant <= minVersion || fallbackLimitReached(hostName, intolerant)) {
    // We can't fall back any further. Assume that intolerance isn't the issue.
    uint32_t tolerant = forgetIntolerance(hostName, port);
    // If we know the server is tolerant at the version, we don't have to
    // gather the telemetry.
    if (intolerant <= tolerant) {
      return false;
    }

    uint32_t fallbackLimitBucket = 0;
    // added if the version has reached the min version.
    if (intolerant <= minVersion) {
      switch (minVersion) {
        case SSL_LIBRARY_VERSION_TLS_1_0:
          fallbackLimitBucket += 1;
          break;
        case SSL_LIBRARY_VERSION_TLS_1_1:
          fallbackLimitBucket += 2;
          break;
        case SSL_LIBRARY_VERSION_TLS_1_2:
          fallbackLimitBucket += 3;
          break;
      }
    }
    // added if the version has reached the fallback limit.
    if (intolerant <= mVersionFallbackLimit) {
      switch (mVersionFallbackLimit) {
        case SSL_LIBRARY_VERSION_TLS_1_0:
          fallbackLimitBucket += 4;
          break;
        case SSL_LIBRARY_VERSION_TLS_1_1:
          fallbackLimitBucket += 8;
          break;
        case SSL_LIBRARY_VERSION_TLS_1_2:
          fallbackLimitBucket += 12;
          break;
      }
    }
    if (fallbackLimitBucket) {
      Telemetry::Accumulate(Telemetry::SSL_FALLBACK_LIMIT_REACHED,
                            fallbackLimitBucket);
    }

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
    entry.strongCipherStatus = StrongCipherStatusUnknown;
  }

  entry.intolerant = intolerant;
  entry.intoleranceReason = intoleranceReason;
  entry.AssertInvariant();
  mTLSIntoleranceInfo.Put(key, entry);

  return true;
}

// returns true if we should retry the handshake
bool
nsSSLIOLayerHelpers::rememberStrongCiphersFailed(const nsACString& hostName,
                                                 int16_t port,
                                                 PRErrorCode intoleranceReason)
{
  nsCString key;
  getSiteKey(hostName, port, key);

  MutexAutoLock lock(mutex);

  IntoleranceEntry entry;
  if (mTLSIntoleranceInfo.Get(key, &entry)) {
    entry.AssertInvariant();
    if (entry.strongCipherStatus != StrongCipherStatusUnknown) {
      // We already know if the server supports a strong cipher.
      return false;
    }
  } else {
    entry.tolerant = 0;
    entry.intolerant = 0;
    entry.intoleranceReason = intoleranceReason;
  }

  entry.strongCipherStatus = StrongCiphersFailed;
  entry.AssertInvariant();
  mTLSIntoleranceInfo.Put(key, entry);

  return true;
}

void
nsSSLIOLayerHelpers::adjustForTLSIntolerance(const nsACString& hostName,
                                             int16_t port,
                                             /*in/out*/ SSLVersionRange& range,
                                             /*out*/ StrongCipherStatus& strongCipherStatus)
{
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
  strongCipherStatus = entry.strongCipherStatus;
}

PRErrorCode
nsSSLIOLayerHelpers::getIntoleranceReason(const nsACString& hostName,
                                          int16_t port)
{
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

static PRStatus
nsSSLIOLayerClose(PRFileDesc* fd)
{
  nsNSSShutDownPreventionLock locker;
  if (!fd)
    return PR_FAILURE;

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] Shutting down socket\n",
         (void*) fd));

  nsNSSSocketInfo* socketInfo = (nsNSSSocketInfo*) fd->secret;
  NS_ASSERTION(socketInfo,"nsNSSSocketInfo was null for an fd");

  return socketInfo->CloseSocketAndDestroy(locker);
}

PRStatus
nsNSSSocketInfo::CloseSocketAndDestroy(
    const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  nsNSSShutDownList::trackSSLSocketClose();

  PRFileDesc* popped = PR_PopIOLayer(mFd, PR_TOP_IO_LAYER);
  NS_ASSERTION(popped &&
               popped->identity == nsSSLIOLayerHelpers::nsSSLIOLayerIdentity,
               "SSL Layer not on top of stack");

  // The plain text layer is not always present - so its not a fatal error
  // if it cannot be removed
  PRFileDesc* poppedPlaintext =
    PR_GetIdentitiesLayer(mFd, nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity);
  if (poppedPlaintext) {
    PR_PopIOLayer(mFd, nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity);
    poppedPlaintext->dtor(poppedPlaintext);
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

#if defined(DEBUG_SSL_VERBOSE) && defined(DUMP_BUFFER)
// Dumps a (potentially binary) buffer using SSM_DEBUG.  (We could have used
// the version in ssltrace.c, but that's specifically tailored to SSLTRACE.)
#define DUMPBUF_LINESIZE 24
static void
nsDumpBuffer(unsigned char* buf, int len)
{
  char hexbuf[DUMPBUF_LINESIZE*3+1];
  char chrbuf[DUMPBUF_LINESIZE+1];
  static const char* hex = "0123456789abcdef";
  int i = 0;
  int l = 0;
  char ch;
  char* c;
  char* h;
  if (len == 0)
    return;
  hexbuf[DUMPBUF_LINESIZE*3] = '\0';
  chrbuf[DUMPBUF_LINESIZE] = '\0';
  (void) memset(hexbuf, 0x20, DUMPBUF_LINESIZE*3);
  (void) memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
  h = hexbuf;
  c = chrbuf;

  while (i < len) {
    ch = buf[i];

    if (l == DUMPBUF_LINESIZE) {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("%s%s\n", hexbuf, chrbuf));
      (void) memset(hexbuf, 0x20, DUMPBUF_LINESIZE*3);
      (void) memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
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
    i++; l++;
  }
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("%s%s\n", hexbuf, chrbuf));
}

#define DEBUG_DUMP_BUFFER(buf,len) nsDumpBuffer(buf,len)
#else
#define DEBUG_DUMP_BUFFER(buf,len)
#endif

class SSLErrorRunnable : public SyncRunnableBase
{
 public:
  SSLErrorRunnable(nsNSSSocketInfo* infoObject,
                   ::mozilla::psm::SSLErrorMessageType errtype,
                   PRErrorCode errorCode)
    : mInfoObject(infoObject)
    , mErrType(errtype)
    , mErrorCode(errorCode)
  {
  }

  virtual void RunOnTargetThread()
  {
    nsHandleSSLError(mInfoObject, mErrType, mErrorCode);
  }

  RefPtr<nsNSSSocketInfo> mInfoObject;
  ::mozilla::psm::SSLErrorMessageType mErrType;
  const PRErrorCode mErrorCode;
};

namespace {

uint32_t tlsIntoleranceTelemetryBucket(PRErrorCode err)
{
  // returns a numeric code for where we track various errors in telemetry
  // only errors that cause version fallback are tracked,
  // so this is also used to determine which errors can cause version fallback
  switch (err) {
    case SSL_ERROR_BAD_MAC_ALERT: return 1;
    case SSL_ERROR_BAD_MAC_READ: return 2;
    case SSL_ERROR_HANDSHAKE_FAILURE_ALERT: return 3;
    case SSL_ERROR_HANDSHAKE_UNEXPECTED_ALERT: return 4;
    case SSL_ERROR_ILLEGAL_PARAMETER_ALERT: return 6;
    case SSL_ERROR_NO_CYPHER_OVERLAP: return 7;
    case SSL_ERROR_UNSUPPORTED_VERSION: return 10;
    case SSL_ERROR_PROTOCOL_VERSION_ALERT: return 11;
    case SSL_ERROR_BAD_HANDSHAKE_HASH_VALUE: return 13;
    case SSL_ERROR_DECODE_ERROR_ALERT: return 14;
    case PR_CONNECT_RESET_ERROR: return 16;
    case PR_END_OF_FILE_ERROR: return 17;
    default: return 0;
  }
}

bool
retryDueToTLSIntolerance(PRErrorCode err, nsNSSSocketInfo* socketInfo)
{
  // This function is supposed to decide which error codes should
  // be used to conclude server is TLS intolerant.
  // Note this only happens during the initial SSL handshake.

  SSLVersionRange range = socketInfo->GetTLSVersionRange();
  nsSSLIOLayerHelpers& helpers = socketInfo->SharedState().IOLayerHelpers();

  if (err == SSL_ERROR_UNSUPPORTED_VERSION &&
      range.min == SSL_LIBRARY_VERSION_TLS_1_0) {
    socketInfo->SetSecurityState(nsIWebProgressListener::STATE_IS_INSECURE |
                                 nsIWebProgressListener::STATE_USES_SSL_3);
  }

  if (err == SSL_ERROR_INAPPROPRIATE_FALLBACK_ALERT) {
    // This is a clear signal that we've fallen back too many versions.  Treat
    // this as a hard failure, but forget any intolerance so that later attempts
    // don't use this version (i.e., range.max) and trigger the error again.

    // First, track the original cause of the version fallback.  This uses the
    // same buckets as the telemetry below, except that bucket 0 will include
    // all cases where there wasn't an original reason.
    PRErrorCode originalReason =
      helpers.getIntoleranceReason(socketInfo->GetHostName(),
                                   socketInfo->GetPort());
    Telemetry::Accumulate(Telemetry::SSL_VERSION_FALLBACK_INAPPROPRIATE,
                          tlsIntoleranceTelemetryBucket(originalReason));

    helpers.forgetIntolerance(socketInfo->GetHostName(),
                              socketInfo->GetPort());

    return false;
  }

  // Disallow PR_CONNECT_RESET_ERROR if fallback limit reached.
  bool fallbackLimitReached =
    helpers.fallbackLimitReached(socketInfo->GetHostName(), range.max);
  if (err == PR_CONNECT_RESET_ERROR && fallbackLimitReached) {
    return false;
  }

  if ((err == SSL_ERROR_NO_CYPHER_OVERLAP || err == PR_END_OF_FILE_ERROR ||
       err == PR_CONNECT_RESET_ERROR) &&
      (!fallbackLimitReached || helpers.mUnrestrictedRC4Fallback) &&
      nsNSSComponent::AreAnyWeakCiphersEnabled()) {
    if (helpers.rememberStrongCiphersFailed(socketInfo->GetHostName(),
                                            socketInfo->GetPort(), err)) {
      Telemetry::Accumulate(Telemetry::SSL_WEAK_CIPHERS_FALLBACK,
                            tlsIntoleranceTelemetryBucket(err));
      return true;
    }
    Telemetry::Accumulate(Telemetry::SSL_WEAK_CIPHERS_FALLBACK, 0);
  }

  // When not using a proxy we'll see a connection reset error.
  // When using a proxy, we'll see an end of file error.

  // Don't allow STARTTLS connections to fall back on connection resets or
  // EOF.
  if ((err == PR_CONNECT_RESET_ERROR || err == PR_END_OF_FILE_ERROR)
      && socketInfo->GetForSTARTTLS()) {
    return false;
  }

  uint32_t reason = tlsIntoleranceTelemetryBucket(err);
  if (reason == 0) {
    return false;
  }

  Telemetry::ID pre;
  Telemetry::ID post;
  switch (range.max) {
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
                                           socketInfo->GetPort(),
                                           range.min, range.max, err)) {
    return false;
  }

  Telemetry::Accumulate(post, reason);

  return true;
}

int32_t
checkHandshake(int32_t bytesTransfered, bool wasReading,
               PRFileDesc* ssl_layer_fd, nsNSSSocketInfo* socketInfo)
{
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

  // Do NOT assume TLS intolerance on a closed connection after bad cert ui was shown.
  // Simply retry.
  // This depends on the fact that Cert UI will not be shown again,
  // should the user override the bad cert.

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
    // The socketInfo->GetErrorCode() check is here to ensure we don't try to
    // do the synchronous dispatch to the main thread unnecessarily after we've
    // already handled a certificate error. (SSLErrorRunnable calls
    // nsHandleSSLError, which has logic to avoid replacing the error message,
    // so without the !socketInfo->GetErrorCode(), it would just be an
    // expensive no-op.)
    if (!wantRetry && mozilla::psm::IsNSSErrorCode(err) &&
        !socketInfo->GetErrorCode()) {
      RefPtr<SyncRunnableBase> runnable(new SSLErrorRunnable(socketInfo,
                                                             PlainErrorMessage,
                                                             err));
      (void) runnable->DispatchToMainThreadAndWait();
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
    if (wasReading)
      bytesTransfered = -1;
  }

  // TLS intolerant servers only cause the first transfer to fail, so let's
  // set the HandshakePending attribute to false so that we don't try the logic
  // above again in a subsequent transfer.
  if (handleHandshakeResultNow) {
    socketInfo->SetHandshakeNotPending();
  }

  if (bytesTransfered < 0) {
    // Remember that we encountered an error so that getSocketInfoIfRunning
    // will correctly cause us to fail if another part of Gecko
    // (erroneously) calls an I/O function (PR_Send/PR_Recv/etc.) again on
    // this socket. Note that we use the original error because if we use
    // PR_CONNECT_RESET_ERROR, we'll repeated try to reconnect.
    if (originalError != PR_WOULD_BLOCK_ERROR && !socketInfo->GetErrorCode()) {
      socketInfo->SetCanceled(originalError, PlainErrorMessage);
    }
    PR_SetError(err, 0);
  }

  return bytesTransfered;
}

} // namespace

static int16_t
nsSSLIOLayerPoll(PRFileDesc* fd, int16_t in_flags, int16_t* out_flags)
{
  nsNSSShutDownPreventionLock locker;

  if (!out_flags) {
    NS_WARNING("nsSSLIOLayerPoll called with null out_flags");
    return 0;
  }

  *out_flags = 0;

  nsNSSSocketInfo* socketInfo =
    getSocketInfoIfRunning(fd, not_reading_or_writing, locker);

  if (!socketInfo) {
    // If we get here, it is probably because certificate validation failed
    // and this is the first I/O operation after the failure.
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] polling SSL socket right after certificate verification failed "
                  "or NSS shutdown or SDR logout %d\n",
             fd, (int) in_flags));

    NS_ASSERTION(in_flags & PR_POLL_EXCEPT,
                 "caller did not poll for EXCEPT (canceled)");
    // Since this poll method cannot return errors, we want the caller to call
    // PR_Send/PR_Recv right away to get the error, so we tell that we are
    // ready for whatever I/O they are asking for. (See getSocketInfoIfRunning).
    *out_flags = in_flags | PR_POLL_EXCEPT; // see also bug 480619
    return in_flags;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
         (socketInfo->IsWaitingForCertVerification()
            ?  "[%p] polling SSL socket during certificate verification using lower %d\n"
            :  "[%p] poll SSL socket using lower %d\n",
         fd, (int) in_flags));

  // We want the handshake to continue during certificate validation, so we
  // don't need to do anything special here. libssl automatically blocks when
  // it reaches any point that would be unsafe to send/receive something before
  // cert validation is complete.
  int16_t result = fd->lower->methods->poll(fd->lower, in_flags, out_flags);
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] poll SSL socket returned %d\n",
                                    (void*) fd, (int) result));
  return result;
}

nsSSLIOLayerHelpers::nsSSLIOLayerHelpers()
  : mTreatUnsafeNegotiationAsBroken(false)
  , mWarnLevelMissingRFC5746(1)
  , mTLSIntoleranceInfo()
  , mFalseStartRequireNPN(false)
  , mUseStaticFallbackList(true)
  , mUnrestrictedRC4Fallback(false)
  , mVersionFallbackLimit(SSL_LIBRARY_VERSION_TLS_1_0)
  , mutex("nsSSLIOLayerHelpers.mutex")
{
}

static int
_PSM_InvalidInt(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static int64_t
_PSM_InvalidInt64(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return -1;
}

static PRStatus
_PSM_InvalidStatus(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return PR_FAILURE;
}

static PRFileDesc*
_PSM_InvalidDesc(void)
{
    PR_ASSERT(!"I/O method is invalid");
    PR_SetError(PR_INVALID_METHOD_ERROR, 0);
    return nullptr;
}

static PRStatus
PSMGetsockname(PRFileDesc* fd, PRNetAddr* addr)
{
  nsNSSShutDownPreventionLock locker;
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing, locker))
    return PR_FAILURE;

  return fd->lower->methods->getsockname(fd->lower, addr);
}

static PRStatus
PSMGetpeername(PRFileDesc* fd, PRNetAddr* addr)
{
  nsNSSShutDownPreventionLock locker;
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing, locker))
    return PR_FAILURE;

  return fd->lower->methods->getpeername(fd->lower, addr);
}

static PRStatus
PSMGetsocketoption(PRFileDesc* fd, PRSocketOptionData* data)
{
  nsNSSShutDownPreventionLock locker;
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing, locker))
    return PR_FAILURE;

  return fd->lower->methods->getsocketoption(fd, data);
}

static PRStatus
PSMSetsocketoption(PRFileDesc* fd, const PRSocketOptionData* data)
{
  nsNSSShutDownPreventionLock locker;
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing, locker))
    return PR_FAILURE;

  return fd->lower->methods->setsocketoption(fd, data);
}

static int32_t
PSMRecv(PRFileDesc* fd, void* buf, int32_t amount, int flags,
        PRIntervalTime timeout)
{
  nsNSSShutDownPreventionLock locker;
  nsNSSSocketInfo* socketInfo = getSocketInfoIfRunning(fd, reading, locker);
  if (!socketInfo)
    return -1;

  if (flags != PR_MSG_PEEK && flags != 0) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return -1;
  }

  int32_t bytesRead = fd->lower->methods->recv(fd->lower, buf, amount, flags,
                                               timeout);

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] read %d bytes\n", (void*) fd,
         bytesRead));

#ifdef DEBUG_SSL_VERBOSE
  DEBUG_DUMP_BUFFER((unsigned char*) buf, bytesRead);
#endif

  return checkHandshake(bytesRead, true, fd, socketInfo);
}

static int32_t
PSMSend(PRFileDesc* fd, const void* buf, int32_t amount, int flags,
        PRIntervalTime timeout)
{
  nsNSSShutDownPreventionLock locker;
  nsNSSSocketInfo* socketInfo = getSocketInfoIfRunning(fd, writing, locker);
  if (!socketInfo)
    return -1;

  if (flags != 0) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return -1;
  }

#ifdef DEBUG_SSL_VERBOSE
  DEBUG_DUMP_BUFFER((unsigned char*) buf, amount);
#endif

  int32_t bytesWritten = fd->lower->methods->send(fd->lower, buf, amount,
                                                  flags, timeout);

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] wrote %d bytes\n",
         fd, bytesWritten));

  return checkHandshake(bytesWritten, false, fd, socketInfo);
}

static PRStatus
PSMBind(PRFileDesc* fd, const PRNetAddr *addr)
{
  nsNSSShutDownPreventionLock locker;
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing, locker))
    return PR_FAILURE;

  return fd->lower->methods->bind(fd->lower, addr);
}

static int32_t
nsSSLIOLayerRead(PRFileDesc* fd, void* buf, int32_t amount)
{
  return PSMRecv(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}

static int32_t
nsSSLIOLayerWrite(PRFileDesc* fd, const void* buf, int32_t amount)
{
  return PSMSend(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}

static PRStatus
PSMConnectcontinue(PRFileDesc* fd, int16_t out_flags)
{
  nsNSSShutDownPreventionLock locker;
  if (!getSocketInfoIfRunning(fd, not_reading_or_writing, locker)) {
    return PR_FAILURE;
  }

  return fd->lower->methods->connectcontinue(fd, out_flags);
}

static int
PSMAvailable(void)
{
  // This is called through PR_Available(), but is not implemented in PSM
  PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
  return -1;
}

static int64_t
PSMAvailable64(void)
{
  // This is called through PR_Available(), but is not implemented in PSM
  PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
  return -1;
}

namespace {

class PrefObserver : public nsIObserver {
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  explicit PrefObserver(nsSSLIOLayerHelpers* aOwner) : mOwner(aOwner) {}

protected:
  virtual ~PrefObserver() {}
private:
  nsSSLIOLayerHelpers* mOwner;
};

} // unnamed namespace

NS_IMPL_ISUPPORTS(PrefObserver, nsIObserver)

NS_IMETHODIMP
PrefObserver::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* someData)
{
  if (nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
    NS_ConvertUTF16toUTF8 prefName(someData);

    if (prefName.EqualsLiteral("security.ssl.treat_unsafe_negotiation_as_broken")) {
      bool enabled;
      Preferences::GetBool("security.ssl.treat_unsafe_negotiation_as_broken", &enabled);
      mOwner->setTreatUnsafeNegotiationAsBroken(enabled);
    } else if (prefName.EqualsLiteral("security.ssl.warn_missing_rfc5746")) {
      int32_t warnLevel = 1;
      Preferences::GetInt("security.ssl.warn_missing_rfc5746", &warnLevel);
      mOwner->setWarnLevelMissingRFC5746(warnLevel);
    } else if (prefName.EqualsLiteral("security.ssl.false_start.require-npn")) {
      mOwner->mFalseStartRequireNPN =
        Preferences::GetBool("security.ssl.false_start.require-npn",
                             FALSE_START_REQUIRE_NPN_DEFAULT);
    } else if (prefName.EqualsLiteral("security.tls.version.fallback-limit")) {
      mOwner->loadVersionFallbackLimit();
    } else if (prefName.EqualsLiteral("security.tls.insecure_fallback_hosts")) {
      nsCString insecureFallbackHosts;
      Preferences::GetCString("security.tls.insecure_fallback_hosts", &insecureFallbackHosts);
      mOwner->setInsecureFallbackSites(insecureFallbackHosts);
    } else if (prefName.EqualsLiteral("security.tls.insecure_fallback_hosts.use_static_list")) {
      mOwner->mUseStaticFallbackList =
        Preferences::GetBool("security.tls.insecure_fallback_hosts.use_static_list", true);
    } else if (prefName.EqualsLiteral("security.tls.unrestricted_rc4_fallback")) {
      mOwner->mUnrestrictedRC4Fallback =
        Preferences::GetBool("security.tls.unrestricted_rc4_fallback", false);
    }
  }
  return NS_OK;
}

static int32_t
PlaintextRecv(PRFileDesc* fd, void* buf, int32_t amount, int flags,
              PRIntervalTime timeout)
{
  // The shutdownlocker is not needed here because it will already be
  // held higher in the stack
  nsNSSSocketInfo* socketInfo = nullptr;

  int32_t bytesRead = fd->lower->methods->recv(fd->lower, buf, amount, flags,
                                               timeout);
  if (fd->identity == nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity)
    socketInfo = (nsNSSSocketInfo*) fd->secret;

  if ((bytesRead > 0) && socketInfo)
    socketInfo->AddPlaintextBytesRead(bytesRead);
  return bytesRead;
}

nsSSLIOLayerHelpers::~nsSSLIOLayerHelpers()
{
  // mPrefObserver will only be set if this->Init was called. The GTest tests
  // do not call Init.
  if (mPrefObserver) {
    Preferences::RemoveObserver(mPrefObserver,
        "security.ssl.treat_unsafe_negotiation_as_broken");
    Preferences::RemoveObserver(mPrefObserver,
        "security.ssl.warn_missing_rfc5746");
    Preferences::RemoveObserver(mPrefObserver,
        "security.ssl.false_start.require-npn");
    Preferences::RemoveObserver(mPrefObserver,
        "security.tls.version.fallback-limit");
    Preferences::RemoveObserver(mPrefObserver,
        "security.tls.insecure_fallback_hosts");
    Preferences::RemoveObserver(mPrefObserver,
        "security.tls.unrestricted_rc4_fallback");
  }
}

nsresult
nsSSLIOLayerHelpers::Init()
{
  if (!nsSSLIOLayerInitialized) {
    nsSSLIOLayerInitialized = true;
    nsSSLIOLayerIdentity = PR_GetUniqueIdentity("NSS layer");
    nsSSLIOLayerMethods  = *PR_GetDefaultIOMethods();

    nsSSLIOLayerMethods.available = (PRAvailableFN) PSMAvailable;
    nsSSLIOLayerMethods.available64 = (PRAvailable64FN) PSMAvailable64;
    nsSSLIOLayerMethods.fsync = (PRFsyncFN) _PSM_InvalidStatus;
    nsSSLIOLayerMethods.seek = (PRSeekFN) _PSM_InvalidInt;
    nsSSLIOLayerMethods.seek64 = (PRSeek64FN) _PSM_InvalidInt64;
    nsSSLIOLayerMethods.fileInfo = (PRFileInfoFN) _PSM_InvalidStatus;
    nsSSLIOLayerMethods.fileInfo64 = (PRFileInfo64FN) _PSM_InvalidStatus;
    nsSSLIOLayerMethods.writev = (PRWritevFN) _PSM_InvalidInt;
    nsSSLIOLayerMethods.accept = (PRAcceptFN) _PSM_InvalidDesc;
    nsSSLIOLayerMethods.listen = (PRListenFN) _PSM_InvalidStatus;
    nsSSLIOLayerMethods.shutdown = (PRShutdownFN) _PSM_InvalidStatus;
    nsSSLIOLayerMethods.recvfrom = (PRRecvfromFN) _PSM_InvalidInt;
    nsSSLIOLayerMethods.sendto = (PRSendtoFN) _PSM_InvalidInt;
    nsSSLIOLayerMethods.acceptread = (PRAcceptreadFN) _PSM_InvalidInt;
    nsSSLIOLayerMethods.transmitfile = (PRTransmitfileFN) _PSM_InvalidInt;
    nsSSLIOLayerMethods.sendfile = (PRSendfileFN) _PSM_InvalidInt;

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
    nsSSLPlaintextLayerMethods  = *PR_GetDefaultIOMethods();
    nsSSLPlaintextLayerMethods.recv = PlaintextRecv;
  }

  bool enabled = false;
  Preferences::GetBool("security.ssl.treat_unsafe_negotiation_as_broken", &enabled);
  setTreatUnsafeNegotiationAsBroken(enabled);

  int32_t warnLevel = 1;
  Preferences::GetInt("security.ssl.warn_missing_rfc5746", &warnLevel);
  setWarnLevelMissingRFC5746(warnLevel);

  mFalseStartRequireNPN =
    Preferences::GetBool("security.ssl.false_start.require-npn",
                         FALSE_START_REQUIRE_NPN_DEFAULT);
  loadVersionFallbackLimit();
  nsCString insecureFallbackHosts;
  Preferences::GetCString("security.tls.insecure_fallback_hosts", &insecureFallbackHosts);
  setInsecureFallbackSites(insecureFallbackHosts);
  mUseStaticFallbackList =
    Preferences::GetBool("security.tls.insecure_fallback_hosts.use_static_list", true);
  mUnrestrictedRC4Fallback =
    Preferences::GetBool("security.tls.unrestricted_rc4_fallback", false);

  mPrefObserver = new PrefObserver(this);
  Preferences::AddStrongObserver(mPrefObserver,
                                 "security.ssl.treat_unsafe_negotiation_as_broken");
  Preferences::AddStrongObserver(mPrefObserver,
                                 "security.ssl.warn_missing_rfc5746");
  Preferences::AddStrongObserver(mPrefObserver,
                                 "security.ssl.false_start.require-npn");
  Preferences::AddStrongObserver(mPrefObserver,
                                 "security.tls.version.fallback-limit");
  Preferences::AddStrongObserver(mPrefObserver,
                                 "security.tls.insecure_fallback_hosts");
  Preferences::AddStrongObserver(mPrefObserver,
                                 "security.tls.unrestricted_rc4_fallback");
  return NS_OK;
}

void
nsSSLIOLayerHelpers::loadVersionFallbackLimit()
{
  // see nsNSSComponent::setEnabledTLSVersions for pref handling rules
  uint32_t limit = Preferences::GetUint("security.tls.version.fallback-limit",
                                        3); // 3 = TLS 1.2
  SSLVersionRange defaults = { SSL_LIBRARY_VERSION_TLS_1_2,
                               SSL_LIBRARY_VERSION_TLS_1_2 };
  SSLVersionRange filledInRange;
  nsNSSComponent::FillTLSVersionRange(filledInRange, limit, limit, defaults);

  mVersionFallbackLimit = filledInRange.max;
}

void
nsSSLIOLayerHelpers::clearStoredData()
{
  MutexAutoLock lock(mutex);
  mInsecureFallbackSites.Clear();
  mTLSIntoleranceInfo.Clear();
}

void
nsSSLIOLayerHelpers::setInsecureFallbackSites(const nsCString& str)
{
  MutexAutoLock lock(mutex);

  mInsecureFallbackSites.Clear();

  if (str.IsEmpty()) {
    return;
  }

  nsCCharSeparatedTokenizer toker(str, ',');

  while (toker.hasMoreTokens()) {
    const nsCSubstring& host = toker.nextToken();
    if (!host.IsEmpty()) {
      mInsecureFallbackSites.PutEntry(host);
    }
  }
}

struct FallbackListComparator
{
  explicit FallbackListComparator(const char* aTarget)
    : mTarget(aTarget)
  {}

  int operator()(const char* aVal) const {
    return strcmp(mTarget, aVal);
  }

private:
  const char* mTarget;
};

static const char* const kFallbackWildcardList[] =
{
  ".kuronekoyamato.co.jp", // bug 1128366
  ".userstorage.mega.co.nz", // bug 1133496
  ".wildcard.test",
};

bool
nsSSLIOLayerHelpers::isInsecureFallbackSite(const nsACString& hostname)
{
  size_t match;
  if (mUseStaticFallbackList) {
    const char* host = PromiseFlatCString(hostname).get();
    if (BinarySearchIf(kIntolerantFallbackList, 0,
          ArrayLength(kIntolerantFallbackList),
          FallbackListComparator(host), &match)) {
      return true;
    }
    for (size_t i = 0; i < ArrayLength(kFallbackWildcardList); ++i) {
      size_t hostLen = hostname.Length();
      const char* target = kFallbackWildcardList[i];
      size_t targetLen = strlen(target);
      if (hostLen > targetLen &&
          !memcmp(host + hostLen - targetLen, target, targetLen)) {
        return true;
      }
    }
  }
  MutexAutoLock lock(mutex);
  return mInsecureFallbackSites.Contains(hostname);
}

void
nsSSLIOLayerHelpers::setTreatUnsafeNegotiationAsBroken(bool broken)
{
  MutexAutoLock lock(mutex);
  mTreatUnsafeNegotiationAsBroken = broken;
}

bool
nsSSLIOLayerHelpers::treatUnsafeNegotiationAsBroken()
{
  MutexAutoLock lock(mutex);
  return mTreatUnsafeNegotiationAsBroken;
}

void
nsSSLIOLayerHelpers::setWarnLevelMissingRFC5746(int32_t level)
{
  MutexAutoLock lock(mutex);
  mWarnLevelMissingRFC5746 = level;
}

int32_t
nsSSLIOLayerHelpers::getWarnLevelMissingRFC5746()
{
  MutexAutoLock lock(mutex);
  return mWarnLevelMissingRFC5746;
}

nsresult
nsSSLIOLayerNewSocket(int32_t family,
                      const char* host,
                      int32_t port,
                      const char* proxyHost,
                      int32_t proxyPort,
                      PRFileDesc** fd,
                      nsISupports** info,
                      bool forSTARTTLS,
                      uint32_t flags)
{

  PRFileDesc* sock = PR_OpenTCPSocket(family);
  if (!sock) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = nsSSLIOLayerAddToSocket(family, host, port, proxyHost, proxyPort,
                                        sock, info, forSTARTTLS, flags);
  if (NS_FAILED(rv)) {
    PR_Close(sock);
    return rv;
  }

  *fd = sock;
  return NS_OK;
}

// Creates CA names strings from (CERTDistNames* caNames)
//
// - arena: arena to allocate strings on
// - caNameStrings: filled with CA names strings on return
// - caNames: CERTDistNames to extract strings from
// - return: SECSuccess if successful; error code otherwise
//
// Note: copied in its entirety from Nova code
static SECStatus
nsConvertCANamesToStrings(PLArenaPool* arena, char** caNameStrings,
                          CERTDistNames* caNames)
{
    SECItem* dername;
    SECStatus rv;
    int headerlen;
    uint32_t contentlen;
    SECItem newitem;
    int n;
    char* namestring;

    for (n = 0; n < caNames->nnames; n++) {
        newitem.data = nullptr;
        dername = &caNames->names[n];

        rv = DER_Lengths(dername, &headerlen, &contentlen);

        if (rv != SECSuccess) {
            goto loser;
        }

        if (headerlen + contentlen != dername->len) {
            // This must be from an enterprise 2.x server, which sent
            // incorrectly formatted der without the outer wrapper of type and
            // length. Fix it up by adding the top level header.
            if (dername->len <= 127) {
                newitem.data = (unsigned char*) PR_Malloc(dername->len + 2);
                if (!newitem.data) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char) 0x30;
                newitem.data[1] = (unsigned char) dername->len;
                (void) memcpy(&newitem.data[2], dername->data, dername->len);
            } else if (dername->len <= 255) {
                newitem.data = (unsigned char*) PR_Malloc(dername->len + 3);
                if (!newitem.data) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char) 0x30;
                newitem.data[1] = (unsigned char) 0x81;
                newitem.data[2] = (unsigned char) dername->len;
                (void) memcpy(&newitem.data[3], dername->data, dername->len);
            } else {
                // greater than 256, better be less than 64k
                newitem.data = (unsigned char*) PR_Malloc(dername->len + 4);
                if (!newitem.data) {
                    goto loser;
                }
                newitem.data[0] = (unsigned char) 0x30;
                newitem.data[1] = (unsigned char) 0x82;
                newitem.data[2] = (unsigned char) ((dername->len >> 8) & 0xff);
                newitem.data[3] = (unsigned char) (dername->len & 0xff);
                memcpy(&newitem.data[4], dername->data, dername->len);
            }
            dername = &newitem;
        }

        namestring = CERT_DerNameToAscii(dername);
        if (!namestring) {
            // XXX - keep going until we fail to convert the name
            caNameStrings[n] = const_cast<char*>("");
        } else {
            caNameStrings[n] = PORT_ArenaStrdup(arena, namestring);
            PR_Free(namestring);
            if (!caNameStrings[n]) {
                goto loser;
            }
        }

        if (newitem.data) {
            PR_Free(newitem.data);
        }
    }

    return SECSuccess;
loser:
    if (newitem.data) {
        PR_Free(newitem.data);
    }
    return SECFailure;
}

// Sets certChoice by reading the preference
//
// If done properly, this function will read the identifier strings for ASK and
// AUTO modes read the selected strings from the preference, compare the
// strings, and determine in which mode it is in. We currently use ASK mode for
// UI apps and AUTO mode for UI-less apps without really asking for
// preferences.
nsresult
nsGetUserCertChoice(SSM_UserCertChoice* certChoice)
{
  char* mode = nullptr;
  nsresult ret;

  NS_ENSURE_ARG_POINTER(certChoice);

  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);

  ret = pref->GetCharPref("security.default_personal_cert", &mode);
  if (NS_FAILED(ret)) {
    goto loser;
  }

  if (PL_strcmp(mode, "Select Automatically") == 0) {
    *certChoice = AUTO;
  } else if (PL_strcmp(mode, "Ask Every Time") == 0) {
    *certChoice = ASK;
  } else {
    // Most likely we see a nickname from a migrated cert.
    // We do not currently support that, ask the user which cert to use.
    *certChoice = ASK;
  }

loser:
  if (mode) {
    free(mode);
  }
  return ret;
}

static bool
hasExplicitKeyUsageNonRepudiation(CERTCertificate* cert)
{
  // There is no extension, v1 or v2 certificate
  if (!cert->extensions)
    return false;

  SECStatus srv;
  SECItem keyUsageItem;
  keyUsageItem.data = nullptr;

  srv = CERT_FindKeyUsageExtension(cert, &keyUsageItem);
  if (srv == SECFailure)
    return false;

  unsigned char keyUsage = keyUsageItem.data[0];
  PORT_Free (keyUsageItem.data);

  return !!(keyUsage & KU_NON_REPUDIATION);
}

class ClientAuthDataRunnable : public SyncRunnableBase
{
public:
  ClientAuthDataRunnable(CERTDistNames* caNames,
                         CERTCertificate** pRetCert,
                         SECKEYPrivateKey** pRetKey,
                         nsNSSSocketInfo* info,
                         CERTCertificate* serverCert)
    : mRV(SECFailure)
    , mErrorCodeToReport(SEC_ERROR_NO_MEMORY)
    , mPRetCert(pRetCert)
    , mPRetKey(pRetKey)
    , mCANames(caNames)
    , mSocketInfo(info)
    , mServerCert(serverCert)
  {
  }

  SECStatus mRV;                        // out
  PRErrorCode mErrorCodeToReport;       // out
  CERTCertificate** const mPRetCert;    // in/out
  SECKEYPrivateKey** const mPRetKey;    // in/out
protected:
  virtual void RunOnTargetThread();
private:
  CERTDistNames* const mCANames;        // in
  nsNSSSocketInfo* const mSocketInfo;   // in
  CERTCertificate* const mServerCert;   // in
};

// This callback function is used to pull client certificate
// information upon server request
//
// - arg: SSL data connection
// - socket: SSL socket we're dealing with
// - caNames: list of CA names
// - pRetCert: returns a pointer to a pointer to a valid certificate if
//             successful; otherwise nullptr
// - pRetKey: returns a pointer to a pointer to the corresponding key if
//            successful; otherwise nullptr
SECStatus
nsNSS_SSLGetClientAuthData(void* arg, PRFileDesc* socket,
                           CERTDistNames* caNames, CERTCertificate** pRetCert,
                           SECKEYPrivateKey** pRetKey)
{
  nsNSSShutDownPreventionLock locker;

  if (!socket || !caNames || !pRetCert || !pRetKey) {
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return SECFailure;
  }

  RefPtr<nsNSSSocketInfo> info(
    reinterpret_cast<nsNSSSocketInfo*>(socket->higher->secret));

  CERTCertificate* serverCert = SSL_PeerCertificate(socket);
  if (!serverCert) {
    NS_NOTREACHED("Missing server certificate should have been detected during "
                  "server cert authentication.");
    PR_SetError(SSL_ERROR_NO_CERTIFICATE, 0);
    return SECFailure;
  }

  if (info->GetJoined()) {
    // We refuse to send a client certificate when there are multiple hostnames
    // joined on this connection, because we only show the user one hostname
    // (mHostName) in the client certificate UI.

    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("[%p] Not returning client cert due to previous join\n", socket));
    *pRetCert = nullptr;
    *pRetKey = nullptr;
    return SECSuccess;
  }

  // XXX: This should be done asynchronously; see bug 696976
  RefPtr<ClientAuthDataRunnable> runnable(
    new ClientAuthDataRunnable(caNames, pRetCert, pRetKey, info, serverCert));
  nsresult rv = runnable->DispatchToMainThreadAndWait();
  if (NS_FAILED(rv)) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return SECFailure;
  }

  if (runnable->mRV != SECSuccess) {
    PR_SetError(runnable->mErrorCodeToReport, 0);
  } else if (*runnable->mPRetCert || *runnable->mPRetKey) {
    // Make joinConnection prohibit joining after we've sent a client cert
    info->SetSentClientCert();
  }

  return runnable->mRV;
}

void
ClientAuthDataRunnable::RunOnTargetThread()
{
  PLArenaPool* arena = nullptr;
  char** caNameStrings;
  ScopedCERTCertificate cert;
  ScopedSECKEYPrivateKey privKey;
  ScopedCERTCertList certList;
  CERTCertListNode* node;
  ScopedCERTCertNicknames nicknames;
  int keyError = 0; // used for private key retrieval error
  SSM_UserCertChoice certChoice;
  int32_t NumberOfCerts = 0;
  void* wincx = mSocketInfo;
  nsresult rv;

  nsCOMPtr<nsIX509Cert> socketClientCert;
  mSocketInfo->GetClientCert(getter_AddRefs(socketClientCert));

  // If a client cert preference was set on the socket info, use that and skip
  // the client cert UI and/or search of the user's past cert decisions.
  if (socketClientCert) {
    cert = socketClientCert->GetCert();
    if (!cert) {
      goto loser;
    }

    // Get the private key
    privKey = PK11_FindKeyByAnyCert(cert.get(), wincx);
    if (!privKey) {
      goto loser;
    }

    *mPRetCert = cert.forget();
    *mPRetKey = privKey.forget();
    mRV = SECSuccess;
    return;
  }

  // create caNameStrings
  arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena) {
    goto loser;
  }

  caNameStrings = (char**) PORT_ArenaAlloc(arena,
                                           sizeof(char*) * (mCANames->nnames));
  if (!caNameStrings) {
    goto loser;
  }

  mRV = nsConvertCANamesToStrings(arena, caNameStrings, mCANames);
  if (mRV != SECSuccess) {
    goto loser;
  }

  // get the preference
  if (NS_FAILED(nsGetUserCertChoice(&certChoice))) {
    goto loser;
  }

  // find valid user cert and key pair
  if (certChoice == AUTO) {
    // automatically find the right cert

    // find all user certs that are valid and for SSL
    certList = CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(),
                                         certUsageSSLClient, false,
                                         true, wincx);
    if (!certList) {
      goto noCert;
    }

    // filter the list to those issued by CAs supported by the server
    mRV = CERT_FilterCertListByCANames(certList.get(), mCANames->nnames,
                                       caNameStrings, certUsageSSLClient);
    if (mRV != SECSuccess) {
      goto noCert;
    }

    // make sure the list is not empty
    node = CERT_LIST_HEAD(certList);
    if (CERT_LIST_END(node, certList)) {
      goto noCert;
    }

    ScopedCERTCertificate low_prio_nonrep_cert;

    // loop through the list until we find a cert with a key
    while (!CERT_LIST_END(node, certList)) {
      // if the certificate has restriction and we do not satisfy it we do not
      // use it
      privKey = PK11_FindKeyByAnyCert(node->cert, wincx);
      if (privKey) {
        if (hasExplicitKeyUsageNonRepudiation(node->cert)) {
          privKey = nullptr;
          // Not a prefered cert
          if (!low_prio_nonrep_cert) { // did not yet find a low prio cert
            low_prio_nonrep_cert = CERT_DupCertificate(node->cert);
          }
        } else {
          // this is a good cert to present
          cert = CERT_DupCertificate(node->cert);
          break;
        }
      }
      keyError = PR_GetError();
      if (keyError == SEC_ERROR_BAD_PASSWORD) {
        // problem with password: bail
        goto loser;
      }

      node = CERT_LIST_NEXT(node);
    }

    if (!cert && low_prio_nonrep_cert) {
      cert = low_prio_nonrep_cert.forget();
      privKey = PK11_FindKeyByAnyCert(cert.get(), wincx);
    }

    if (!cert) {
      goto noCert;
    }
  } else { // Not Auto => ask
    // Get the SSL Certificate

    nsXPIDLCString hostname;
    mSocketInfo->GetHostName(getter_Copies(hostname));

    RefPtr<nsClientAuthRememberService> cars =
      mSocketInfo->SharedState().GetClientAuthRememberService();

    bool hasRemembered = false;
    nsCString rememberedDBKey;
    if (cars) {
      bool found;
      rv = cars->HasRememberedDecision(hostname, mServerCert,
        rememberedDBKey, &found);
      if (NS_SUCCEEDED(rv) && found) {
        hasRemembered = true;
      }
    }

    bool canceled = false;

    if (hasRemembered) {
      if (rememberedDBKey.IsEmpty()) {
        canceled = true;
      } else {
        nsCOMPtr<nsIX509CertDB> certdb;
        certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
        if (certdb) {
          nsCOMPtr<nsIX509Cert> found_cert;
          nsresult find_rv =
            certdb->FindCertByDBKey(rememberedDBKey.get(), nullptr,
            getter_AddRefs(found_cert));
          if (NS_SUCCEEDED(find_rv) && found_cert) {
            nsNSSCertificate* obj_cert =
              reinterpret_cast<nsNSSCertificate*>(found_cert.get());
            if (obj_cert) {
              cert = obj_cert->GetCert();
            }
          }

          if (!cert) {
            hasRemembered = false;
          }
        }
      }
    }

    if (!hasRemembered) {
      // user selects a cert to present
      nsIClientAuthDialogs* dialogs = nullptr;
      int32_t selectedIndex = -1;
      char16_t** certNicknameList = nullptr;
      char16_t** certDetailsList = nullptr;

      // find all user certs that are for SSL
      // note that we are allowing expired certs in this list
      certList = CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(),
        certUsageSSLClient, false,
        false, wincx);
      if (!certList) {
        goto noCert;
      }

      if (mCANames->nnames != 0) {
        // filter the list to those issued by CAs supported by the server
        mRV = CERT_FilterCertListByCANames(certList.get(),
                                           mCANames->nnames,
                                           caNameStrings,
                                           certUsageSSLClient);
        if (mRV != SECSuccess) {
          goto loser;
        }
      }

      if (CERT_LIST_END(CERT_LIST_HEAD(certList), certList)) {
        // list is empty - no matching certs
        goto noCert;
      }

      // filter it further for hostname restriction
      node = CERT_LIST_HEAD(certList.get());
      while (!CERT_LIST_END(node, certList.get())) {
        ++NumberOfCerts;
        node = CERT_LIST_NEXT(node);
      }
      if (CERT_LIST_END(CERT_LIST_HEAD(certList.get()), certList.get())) {
        goto noCert;
      }

      nicknames = getNSSCertNicknamesFromCertList(certList.get());

      if (!nicknames) {
        goto loser;
      }

      NS_ASSERTION(nicknames->numnicknames == NumberOfCerts, "nicknames->numnicknames != NumberOfCerts");

      // Get CN and O of the subject and O of the issuer
      UniquePtr<char, void(&)(void*)>
        ccn(CERT_GetCommonName(&mServerCert->subject), PORT_Free);
      NS_ConvertUTF8toUTF16 cn(ccn.get());

      int32_t port;
      mSocketInfo->GetPort(&port);

      nsString cn_host_port;
      if (ccn && strcmp(ccn.get(), hostname) == 0) {
        cn_host_port.Append(cn);
        cn_host_port.Append(':');
        cn_host_port.AppendInt(port);
      } else {
        cn_host_port.Append(cn);
        cn_host_port.AppendLiteral(" (");
        cn_host_port.Append(':');
        cn_host_port.AppendInt(port);
        cn_host_port.Append(')');
      }

      char* corg = CERT_GetOrgName(&mServerCert->subject);
      NS_ConvertUTF8toUTF16 org(corg);
      if (corg) PORT_Free(corg);

      char* cissuer = CERT_GetOrgName(&mServerCert->issuer);
      NS_ConvertUTF8toUTF16 issuer(cissuer);
      if (cissuer) PORT_Free(cissuer);

      certNicknameList =
        (char16_t**)moz_xmalloc(sizeof(char16_t*)* nicknames->numnicknames);
      if (!certNicknameList)
        goto loser;
      certDetailsList =
        (char16_t**)moz_xmalloc(sizeof(char16_t*)* nicknames->numnicknames);
      if (!certDetailsList) {
        free(certNicknameList);
        goto loser;
      }

      int32_t CertsToUse;
      for (CertsToUse = 0, node = CERT_LIST_HEAD(certList);
        !CERT_LIST_END(node, certList) && CertsToUse < nicknames->numnicknames;
        node = CERT_LIST_NEXT(node)
        ) {
        RefPtr<nsNSSCertificate> tempCert(nsNSSCertificate::Create(node->cert));

        if (!tempCert)
          continue;

        NS_ConvertUTF8toUTF16 i_nickname(nicknames->nicknames[CertsToUse]);
        nsAutoString nickWithSerial, details;

        if (NS_FAILED(tempCert->FormatUIStrings(i_nickname, nickWithSerial, details)))
          continue;

        certNicknameList[CertsToUse] = ToNewUnicode(nickWithSerial);
        if (!certNicknameList[CertsToUse])
          continue;
        certDetailsList[CertsToUse] = ToNewUnicode(details);
        if (!certDetailsList[CertsToUse]) {
          free(certNicknameList[CertsToUse]);
          continue;
        }

        ++CertsToUse;
      }

      // Throw up the client auth dialog and get back the index of the selected cert
      nsresult rv = getNSSDialogs((void**)&dialogs,
        NS_GET_IID(nsIClientAuthDialogs),
        NS_CLIENTAUTHDIALOGS_CONTRACTID);

      if (NS_FAILED(rv)) {
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certNicknameList);
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certDetailsList);
        goto loser;
      }

      {
        nsPSMUITracker tracker;
        if (tracker.isUIForbidden()) {
          rv = NS_ERROR_NOT_AVAILABLE;
        } else {
          rv = dialogs->ChooseCertificate(mSocketInfo, cn_host_port.get(),
            org.get(), issuer.get(),
            (const char16_t**)certNicknameList,
            (const char16_t**)certDetailsList,
            CertsToUse, &selectedIndex, &canceled);
        }
      }

      NS_RELEASE(dialogs);
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certNicknameList);
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(CertsToUse, certDetailsList);

      if (NS_FAILED(rv)) goto loser;

      // even if the user has canceled, we want to remember that, to avoid repeating prompts
      bool wantRemember = false;
      mSocketInfo->GetRememberClientAuthCertificate(&wantRemember);

      int i;
      if (!canceled)
      for (i = 0, node = CERT_LIST_HEAD(certList);
        !CERT_LIST_END(node, certList);
        ++i, node = CERT_LIST_NEXT(node)) {

        if (i == selectedIndex) {
          cert = CERT_DupCertificate(node->cert);
          break;
        }
      }

      if (cars && wantRemember) {
        cars->RememberDecision(hostname, mServerCert,
          canceled ? nullptr : cert.get());
      }
    }

    if (canceled) { rv = NS_ERROR_NOT_AVAILABLE; goto loser; }

    if (!cert) {
      goto loser;
    }

    // go get the private key
    privKey = PK11_FindKeyByAnyCert(cert.get(), wincx);
    if (!privKey) {
      keyError = PR_GetError();
      if (keyError == SEC_ERROR_BAD_PASSWORD) {
        // problem with password: bail
        goto loser;
      } else {
        goto noCert;
      }
    }
  }
  goto done;

noCert:
loser:
  if (mRV == SECSuccess) {
    mRV = SECFailure;
  }
done:
  int error = PR_GetError();

  if (arena) {
    PORT_FreeArena(arena, false);
  }

  *mPRetCert = cert.forget();
  *mPRetKey = privKey.forget();

  if (mRV == SECFailure) {
    mErrorCodeToReport = error;
  }
}

static PRFileDesc*
nsSSLIOLayerImportFD(PRFileDesc* fd,
                     nsNSSSocketInfo* infoObject,
                     const char* host)
{
  nsNSSShutDownPreventionLock locker;
  PRFileDesc* sslSock = SSL_ImportFD(nullptr, fd);
  if (!sslSock) {
    NS_ASSERTION(false, "NSS: Error importing socket");
    return nullptr;
  }
  SSL_SetPKCS11PinArg(sslSock, (nsIInterfaceRequestor*) infoObject);
  SSL_HandshakeCallback(sslSock, HandshakeCallback, infoObject);
  SSL_SetCanFalseStartCallback(sslSock, CanFalseStartCallback, infoObject);

  // Disable this hook if we connect anonymously. See bug 466080.
  uint32_t flags = 0;
  infoObject->GetProviderFlags(&flags);
  if (flags & nsISocketProvider::ANONYMOUS_CONNECT) {
      SSL_GetClientAuthDataHook(sslSock, nullptr, infoObject);
  } else {
      SSL_GetClientAuthDataHook(sslSock,
                            (SSLGetClientAuthData) nsNSS_SSLGetClientAuthData,
                            infoObject);
  }
  if (flags & nsISocketProvider::MITM_OK) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("[%p] nsSSLIOLayerImportFD: bypass authentication flag\n", fd));
    infoObject->SetBypassAuthentication(true);
  }
  if (SECSuccess != SSL_AuthCertificateHook(sslSock, AuthCertificateHook,
                                            infoObject)) {
    NS_NOTREACHED("failed to configure AuthCertificateHook");
    goto loser;
  }

  if (SECSuccess != SSL_SetURL(sslSock, host)) {
    NS_NOTREACHED("SSL_SetURL failed");
    goto loser;
  }

  // This is an optimization to make sure the identity info dataset is parsed
  // and loaded on a separate thread and can be overlapped with network latency.
  EnsureServerVerificationInitialized();

  return sslSock;
loser:
  if (sslSock) {
    PR_Close(sslSock);
  }
  return nullptr;
}

static nsresult
nsSSLIOLayerSetOptions(PRFileDesc* fd, bool forSTARTTLS,
                       const char* proxyHost, const char* host, int32_t port,
                       nsNSSSocketInfo* infoObject)
{
  nsNSSShutDownPreventionLock locker;
  if (forSTARTTLS || proxyHost) {
    if (SECSuccess != SSL_OptionSet(fd, SSL_SECURITY, false)) {
      return NS_ERROR_FAILURE;
    }
  }

  SSLVersionRange range;
  if (SSL_VersionRangeGet(fd, &range) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  uint16_t maxEnabledVersion = range.max;
  StrongCipherStatus strongCiphersStatus = StrongCipherStatusUnknown;
  infoObject->SharedState().IOLayerHelpers()
    .adjustForTLSIntolerance(infoObject->GetHostName(), infoObject->GetPort(),
                             range, strongCiphersStatus);
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
         ("[%p] nsSSLIOLayerSetOptions: using TLS version range (0x%04x,0x%04x)%s\n",
          fd, static_cast<unsigned int>(range.min),
              static_cast<unsigned int>(range.max),
          strongCiphersStatus == StrongCiphersFailed ? " with weak ciphers" : ""));

  if (SSL_VersionRangeSet(fd, &range) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  infoObject->SetTLSVersionRange(range);

  if (strongCiphersStatus == StrongCiphersFailed) {
    nsNSSComponent::UseWeakCiphersOnSocket(fd);
  }

  // when adjustForTLSIntolerance tweaks the maximum version downward,
  // we tell the server using this SCSV so they can detect a downgrade attack
  if (range.max < maxEnabledVersion) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("[%p] nsSSLIOLayerSetOptions: enabling TLS_FALLBACK_SCSV\n", fd));
    if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_FALLBACK_SCSV, true)) {
      return NS_ERROR_FAILURE;
    }
  }

  bool enabled = infoObject->SharedState().IsOCSPStaplingEnabled();
  if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_OCSP_STAPLING, enabled)) {
    return NS_ERROR_FAILURE;
  }

  if (SECSuccess != SSL_OptionSet(fd, SSL_HANDSHAKE_AS_CLIENT, true)) {
    return NS_ERROR_FAILURE;
  }

  // Set the Peer ID so that SSL proxy connections work properly and to
  // separate anonymous and/or private browsing connections.
  uint32_t flags = infoObject->GetProviderFlags();
  nsAutoCString peerId;
  if (flags & nsISocketProvider::ANONYMOUS_CONNECT) { // See bug 466080
    peerId.AppendLiteral("anon:");
  }
  if (flags & nsISocketProvider::NO_PERMANENT_STORAGE) {
    peerId.AppendLiteral("private:");
  }
  if (flags & nsISocketProvider::MITM_OK) {
    peerId.AppendLiteral("bypassAuth:");
  }
  peerId.Append(host);
  peerId.Append(':');
  peerId.AppendInt(port);
  if (SECSuccess != SSL_SetSockPeerID(fd, peerId.get())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsSSLIOLayerAddToSocket(int32_t family,
                        const char* host,
                        int32_t port,
                        const char* proxyHost,
                        int32_t proxyPort,
                        PRFileDesc* fd,
                        nsISupports** info,
                        bool forSTARTTLS,
                        uint32_t providerFlags)
{
  nsNSSShutDownPreventionLock locker;
  PRFileDesc* layer = nullptr;
  PRFileDesc* plaintextLayer = nullptr;
  nsresult rv;
  PRStatus stat;

  SharedSSLState* sharedState =
    providerFlags & nsISocketProvider::NO_PERMANENT_STORAGE ? PrivateSSLState() : PublicSSLState();
  nsNSSSocketInfo* infoObject = new nsNSSSocketInfo(*sharedState, providerFlags);
  if (!infoObject) return NS_ERROR_FAILURE;

  NS_ADDREF(infoObject);
  infoObject->SetForSTARTTLS(forSTARTTLS);
  infoObject->SetHostName(host);
  infoObject->SetPort(port);

  // A plaintext observer shim is inserted so we can observe some protocol
  // details without modifying nss
  plaintextLayer = PR_CreateIOLayerStub(nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity,
                                        &nsSSLIOLayerHelpers::nsSSLPlaintextLayerMethods);
  if (plaintextLayer) {
    plaintextLayer->secret = (PRFilePrivate*) infoObject;
    stat = PR_PushIOLayer(fd, PR_TOP_IO_LAYER, plaintextLayer);
    if (stat == PR_FAILURE) {
      plaintextLayer->dtor(plaintextLayer);
      plaintextLayer = nullptr;
    }
  }

  PRFileDesc* sslSock = nsSSLIOLayerImportFD(fd, infoObject, host);
  if (!sslSock) {
    NS_ASSERTION(false, "NSS: Error importing socket");
    goto loser;
  }

  infoObject->SetFileDescPtr(sslSock);

  rv = nsSSLIOLayerSetOptions(sslSock, forSTARTTLS, proxyHost, host, port,
                              infoObject);

  if (NS_FAILED(rv))
    goto loser;

  // Now, layer ourselves on top of the SSL socket...
  layer = PR_CreateIOLayerStub(nsSSLIOLayerHelpers::nsSSLIOLayerIdentity,
                               &nsSSLIOLayerHelpers::nsSSLIOLayerMethods);
  if (!layer)
    goto loser;

  layer->secret = (PRFilePrivate*) infoObject;
  stat = PR_PushIOLayer(sslSock, PR_GetLayersIdentity(sslSock), layer);

  if (stat == PR_FAILURE) {
    goto loser;
  }

  nsNSSShutDownList::trackSSLSocketCreate();

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] Socket set up\n", (void*) sslSock));
  infoObject->QueryInterface(NS_GET_IID(nsISupports), (void**) (info));

  // We are going use a clear connection first //
  if (forSTARTTLS || proxyHost) {
    infoObject->SetHandshakeNotPending();
  }

  infoObject->SharedState().NoteSocketCreated();

  return NS_OK;
 loser:
  NS_IF_RELEASE(infoObject);
  if (layer) {
    layer->dtor(layer);
  }
  if (plaintextLayer) {
    PR_PopIOLayer(fd, nsSSLIOLayerHelpers::nsSSLPlaintextLayerIdentity);
    plaintextLayer->dtor(plaintextLayer);
  }
  return NS_ERROR_FAILURE;
}

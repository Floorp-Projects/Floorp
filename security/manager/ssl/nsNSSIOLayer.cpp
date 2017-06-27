/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSIOLayer.h"

#include <algorithm>

#include "NSSCertDBTrustDomain.h"
#include "NSSErrorsService.h"
#include "PSMRunnable.h"
#include "SSLServerCertVerification.h"
#include "ScopedNSSTypes.h"
#include "SharedSSLState.h"
#include "keyhi.h"
#include "mozilla/Casting.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "mozilla/Move.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "nsArray.h"
#include "nsArrayUtils.h"
#include "nsCRT.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsClientAuthRemember.h"
#include "nsContentUtils.h"
#include "nsIClientAuthDialogs.h"
#include "nsIConsoleService.h"
#include "nsIPrefService.h"
#include "nsISocketProvider.h"
#include "nsIWebProgressListener.h"
#include "nsNSSCertHelper.h"
#include "nsNSSComponent.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "pkix/pkixtypes.h"
#include "prmem.h"
#include "prnetdb.h"
#include "secder.h"
#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

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

#define MAX_ALPN_LENGTH 255

void
getSiteKey(const nsACString& hostName, uint16_t port,
           /*out*/ nsACString& key)
{
  key = hostName;
  key.AppendASCII(":");
  key.AppendInt(port);
}

} // unnamed namespace

extern LazyLogModule gPIPNSSLog;

nsNSSSocketInfo::nsNSSSocketInfo(SharedSSLState& aState, uint32_t providerFlags)
  : mFd(nullptr),
    mCertVerificationState(before_cert_verification),
    mSharedState(aState),
    mForSTARTTLS(false),
    mHandshakePending(true),
    mRememberClientAuthCertificate(false),
    mPreliminaryHandshakeDone(false),
    mNPNCompleted(false),
    mEarlyDataAccepted(false),
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
nsNSSSocketInfo::GetAlpnEarlySelection(nsACString& aAlpnSelected)
{
  aAlpnSelected.Truncate();

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown() || isPK11LoggedOut()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

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
nsNSSSocketInfo::GetEarlyDataAccepted(bool* aAccepted)
{
  *aAccepted = mEarlyDataAccepted;
  return NS_OK;
}

void
nsNSSSocketInfo::SetEarlyDataAccepted(bool aAccepted)
{
  mEarlyDataAccepted = aAccepted;
}

NS_IMETHODIMP
nsNSSSocketInfo::DriveHandshake()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown() || isPK11LoggedOut()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (!mFd) {
    return NS_ERROR_FAILURE;
  }
  PRErrorCode errorCode = GetErrorCode();
  if (errorCode) {
    return GetXPCOMFromNSSError(errorCode);
  }

  SECStatus rv = SSL_ForceHandshake(mFd);

  if (rv != SECSuccess) {
    errorCode = PR_GetError();
    if (errorCode == PR_WOULD_BLOCK_ERROR) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    SetCanceled(errorCode, SSLErrorMessageType::Plain);
    return GetXPCOMFromNSSError(errorCode);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::IsAcceptableForHost(const nsACString& hostname, bool* _retval)
{
  NS_ENSURE_ARG(_retval);

  *_retval = false;

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

  UniqueCERTCertificate nssCert;

  nsCOMPtr<nsIX509Cert> cert;
  if (NS_FAILED(SSLStatus()->GetServerCert(getter_AddRefs(cert)))) {
    return NS_OK;
  }
  if (cert) {
    nssCert.reset(cert->GetCert());
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
  CertVerifier::Flags flags = CertVerifier::FLAG_LOCAL_ONLY;
  UniqueCERTCertList unusedBuiltChain;
  mozilla::pkix::Result result =
    certVerifier->VerifySSLServerCert(nssCert,
                                      nullptr, // stapledOCSPResponse
                                      nullptr, // sctsFromTLSExtension
                                      mozilla::pkix::Now(),
                                      nullptr, // pinarg
                                      hostname,
                                      unusedBuiltChain,
                                      nullptr, // no peerCertChain
                                      false, // save intermediates
                                      flags);
  if (result != mozilla::pkix::Success) {
    return NS_OK;
  }

  // All tests pass
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::TestJoinConnection(const nsACString& npnProtocol,
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

  IsAcceptableForHost(hostname, _retval); // sets _retval
  return NS_OK;
}

NS_IMETHODIMP
nsNSSSocketInfo::JoinConnection(const nsACString& npnProtocol,
                                const nsACString& hostname,
                                int32_t port,
                                bool* _retval)
{
  nsresult rv = TestJoinConnection(npnProtocol, hostname, port, _retval);
  if (NS_SUCCEEDED(rv) && *_retval) {
    // All tests pass - this is joinable
    mJoined = true;
  }
  return rv;
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
        BitwiseCast<const unsigned char*, const char*>(npnList.get()),
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
  MOZ_ASSERT(mCertVerificationState != waiting_for_cert_verification,
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
  MOZ_ASSERT(mCertVerificationState == waiting_for_cert_verification,
             "Invalid state transition to cert_verification_finished");

  if (mFd) {
    SECStatus rv = SSL_AuthCertificateComplete(mFd, errorCode);
    // Only replace errorCode if there was originally no error
    if (rv != SECSuccess && errorCode == 0) {
      errorCode = PR_GetError();
      errorMessageType = SSLErrorMessageType::Plain;
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
  socketInfo->SetCanceled(err, SSLErrorMessageType::Plain);
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
  } else {
    entry.tolerant = tolerant;
    entry.intolerant = 0;
    entry.intoleranceReason = 0;
  }

  entry.AssertInvariant();

  mTLSIntoleranceInfo.Put(key, entry);
}

void
nsSSLIOLayerHelpers::forgetIntolerance(const nsACString& hostName,
                                       int16_t port)
{
  nsCString key;
  getSiteKey(hostName, port, key);

  MutexAutoLock lock(mutex);

  IntoleranceEntry entry;
  if (mTLSIntoleranceInfo.Get(key, &entry)) {
    entry.AssertInvariant();

    entry.intolerant = 0;
    entry.intoleranceReason = 0;

    entry.AssertInvariant();
    mTLSIntoleranceInfo.Put(key, entry);
  }
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
  mTLSIntoleranceInfo.Put(key, entry);

  return true;
}

void
nsSSLIOLayerHelpers::adjustForTLSIntolerance(const nsACString& hostName,
                                             int16_t port,
                                             /*in/out*/ SSLVersionRange& range)
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
  MOZ_ASSERT(socketInfo, "nsNSSSocketInfo was null for an fd");

  return socketInfo->CloseSocketAndDestroy(locker);
}

PRStatus
nsNSSSocketInfo::CloseSocketAndDestroy(
    const nsNSSShutDownPreventionLock& /*proofOfLock*/)
{
  PRFileDesc* popped = PR_PopIOLayer(mFd, PR_TOP_IO_LAYER);
  MOZ_ASSERT(popped &&
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
    case SSL_ERROR_INTERNAL_ERROR_ALERT: return 18;
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
    PRErrorCode originalReason =
      helpers.getIntoleranceReason(socketInfo->GetHostName(),
                                   socketInfo->GetPort());
    Telemetry::Accumulate(Telemetry::SSL_VERSION_FALLBACK_INAPPROPRIATE,
                          tlsIntoleranceTelemetryBucket(originalReason));

    helpers.forgetIntolerance(socketInfo->GetHostName(),
                              socketInfo->GetPort());

    return false;
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
                                           socketInfo->GetPort(),
                                           range.min, range.max, err)) {
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

static void
reportHandshakeResult(int32_t bytesTransferred, bool wasReading, PRErrorCode err)
{
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
    MOZ_ASSERT(bucket > 0);   // SSL_ERROR_EXPORT_ONLY_SERVER isn't used.
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

  Telemetry::Accumulate(Telemetry::SSL_HANDSHAKE_RESULT, bucket);
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
      RefPtr<SyncRunnableBase> runnable(
        new SSLErrorRunnable(socketInfo, SSLErrorMessageType::Plain, err));
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
    // Report the result once for each handshake. Note that this does not
    // get handshakes which are cancelled before any reads or writes
    // happen.
    reportHandshakeResult(bytesTransfered, wasReading, originalError);
    socketInfo->SetHandshakeNotPending();
  }

  if (bytesTransfered < 0) {
    // Remember that we encountered an error so that getSocketInfoIfRunning
    // will correctly cause us to fail if another part of Gecko
    // (erroneously) calls an I/O function (PR_Send/PR_Recv/etc.) again on
    // this socket. Note that we use the original error because if we use
    // PR_CONNECT_RESET_ERROR, we'll repeated try to reconnect.
    if (originalError != PR_WOULD_BLOCK_ERROR && !socketInfo->GetErrorCode()) {
      socketInfo->SetCanceled(originalError, SSLErrorMessageType::Plain);
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

    MOZ_ASSERT(in_flags & PR_POLL_EXCEPT,
               "Caller did not poll for EXCEPT (canceled)");
    // Since this poll method cannot return errors, we want the caller to call
    // PR_Send/PR_Recv right away to get the error, so we tell that we are
    // ready for whatever I/O they are asking for. (See getSocketInfoIfRunning).
    *out_flags = in_flags | PR_POLL_EXCEPT; // see also bug 480619
    return in_flags;
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
          (socketInfo->IsWaitingForCertVerification()
             ?  "[%p] polling SSL socket during certificate verification using lower %d\n"
             :  "[%p] poll SSL socket using lower %d\n",
           fd, (int) in_flags));

  // We want the handshake to continue during certificate validation, so we
  // don't need to do anything special here. libssl automatically blocks when
  // it reaches any point that would be unsafe to send/receive something before
  // cert validation is complete.
  int16_t result = fd->lower->methods->poll(fd->lower, in_flags, out_flags);
  MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
          ("[%p] poll SSL socket returned %d\n", (void*) fd, (int) result));
  return result;
}

nsSSLIOLayerHelpers::nsSSLIOLayerHelpers()
  : mTreatUnsafeNegotiationAsBroken(false)
  , mTLSIntoleranceInfo()
  , mVersionFallbackLimit(SSL_LIBRARY_VERSION_TLS_1_0)
  , mutex("nsSSLIOLayerHelpers.mutex")
{
}

static int
_PSM_InvalidInt(void)
{
  MOZ_ASSERT_UNREACHABLE("I/O method is invalid");
  PR_SetError(PR_INVALID_METHOD_ERROR, 0);
  return -1;
}

static int64_t
_PSM_InvalidInt64(void)
{
  MOZ_ASSERT_UNREACHABLE("I/O method is invalid");
  PR_SetError(PR_INVALID_METHOD_ERROR, 0);
  return -1;
}

static PRStatus
_PSM_InvalidStatus(void)
{
  MOZ_ASSERT_UNREACHABLE("I/O method is invalid");
  PR_SetError(PR_INVALID_METHOD_ERROR, 0);
  return PR_FAILURE;
}

static PRFileDesc*
_PSM_InvalidDesc(void)
{
  MOZ_ASSERT_UNREACHABLE("I/O method is invalid");
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

  MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
          ("[%p] read %d bytes\n", (void*) fd, bytesRead));

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

  MOZ_LOG(gPIPNSSLog, LogLevel::Verbose,
          ("[%p] wrote %d bytes\n", fd, bytesWritten));

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
    } else if (prefName.EqualsLiteral("security.tls.version.fallback-limit")) {
      mOwner->loadVersionFallbackLimit();
    } else if (prefName.EqualsLiteral("security.tls.insecure_fallback_hosts")) {
      // Changes to the whitelist on the public side will update the pref.
      // Don't propagate the changes to the private side.
      if (mOwner->isPublic()) {
        mOwner->initInsecureFallbackSites();
      }
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
        "security.tls.version.fallback-limit");
    Preferences::RemoveObserver(mPrefObserver,
        "security.tls.insecure_fallback_hosts");
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

  loadVersionFallbackLimit();
  initInsecureFallbackSites();

  mPrefObserver = new PrefObserver(this);
  Preferences::AddStrongObserver(mPrefObserver,
                                 "security.ssl.treat_unsafe_negotiation_as_broken");
  Preferences::AddStrongObserver(mPrefObserver,
                                 "security.tls.version.fallback-limit");
  Preferences::AddStrongObserver(mPrefObserver,
                                 "security.tls.insecure_fallback_hosts");
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
  if (filledInRange.max < SSL_LIBRARY_VERSION_TLS_1_2) {
    filledInRange.max = SSL_LIBRARY_VERSION_TLS_1_2;
  }

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
    const nsACString& host = toker.nextToken();
    if (!host.IsEmpty()) {
      mInsecureFallbackSites.PutEntry(host);
    }
  }
}

void
nsSSLIOLayerHelpers::initInsecureFallbackSites()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCString insecureFallbackHosts;
  Preferences::GetCString("security.tls.insecure_fallback_hosts",
                          &insecureFallbackHosts);
  setInsecureFallbackSites(insecureFallbackHosts);
}

bool
nsSSLIOLayerHelpers::isPublic() const
{
  return this == &PublicSSLState()->IOLayerHelpers();
}

class FallbackPrefRemover final : public Runnable
{
public:
  explicit FallbackPrefRemover(const nsACString& aHost)
    : mozilla::Runnable("FallbackPrefRemover")
    , mHost(aHost)
  {}
  NS_IMETHOD Run() override;
private:
  nsCString mHost;
};

NS_IMETHODIMP
FallbackPrefRemover::Run()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCString oldValue;
  Preferences::GetCString("security.tls.insecure_fallback_hosts", &oldValue);
  nsCCharSeparatedTokenizer toker(oldValue, ',');
  nsCString newValue;
  while (toker.hasMoreTokens()) {
    const nsACString& host = toker.nextToken();
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

void
nsSSLIOLayerHelpers::removeInsecureFallbackSite(const nsACString& hostname,
                                                uint16_t port)
{
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

bool
nsSSLIOLayerHelpers::isInsecureFallbackSite(const nsACString& hostname)
{
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

nsresult
nsSSLIOLayerNewSocket(int32_t family,
                      const char* host,
                      int32_t port,
                      nsIProxyInfo *proxy,
                      const OriginAttributes& originAttributes,
                      PRFileDesc** fd,
                      nsISupports** info,
                      bool forSTARTTLS,
                      uint32_t flags)
{

  PRFileDesc* sock = PR_OpenTCPSocket(family);
  if (!sock) return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = nsSSLIOLayerAddToSocket(family, host, port, proxy,
                                        originAttributes, sock, info,
                                        forSTARTTLS, flags);
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
nsConvertCANamesToStrings(const UniquePLArenaPool& arena, char** caNameStrings,
                          CERTDistNames* caNames)
{
    MOZ_ASSERT(arena.get());
    MOZ_ASSERT(caNameStrings);
    MOZ_ASSERT(caNames);
    if (!arena.get() || !caNameStrings || !caNames) {
        PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
        return SECFailure;
    }

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
            caNameStrings[n] = PORT_ArenaStrdup(arena.get(), namestring);
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

// Possible behaviors for choosing a cert for client auth.
enum class UserCertChoice {
  // Ask the user to choose a cert.
  Ask = 0,
  // Automatically choose a cert.
  Auto = 1,
};

// Returns the most appropriate user cert choice based on the value of the
// security.default_personal_cert preference.
UserCertChoice
nsGetUserCertChoice()
{
  nsAutoCString value;
  nsresult rv = Preferences::GetCString("security.default_personal_cert", &value);
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
                         const UniqueCERTCertificate& serverCert)
    : mRV(SECFailure)
    , mErrorCodeToReport(SEC_ERROR_NO_MEMORY)
    , mPRetCert(pRetCert)
    , mPRetKey(pRetKey)
    , mCANames(caNames)
    , mSocketInfo(info)
    , mServerCert(serverCert.get())
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
    BitwiseCast<nsNSSSocketInfo*, PRFilePrivate*>(socket->higher->secret));

  UniqueCERTCertificate serverCert(SSL_PeerCertificate(socket));
  if (!serverCert) {
    MOZ_ASSERT_UNREACHABLE(
      "Missing server cert should have been detected during server cert auth.");
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
  // We check the value of a pref in this runnable, so this runnable should only
  // be run on the main thread.
  MOZ_ASSERT(NS_IsMainThread());

  UniquePLArenaPool arena;
  char** caNameStrings;
  UniqueCERTCertificate cert;
  UniqueSECKEYPrivateKey privKey;
  void* wincx = mSocketInfo;
  nsresult rv;

  nsCOMPtr<nsIX509Cert> socketClientCert;
  mSocketInfo->GetClientCert(getter_AddRefs(socketClientCert));

  // If a client cert preference was set on the socket info, use that and skip
  // the client cert UI and/or search of the user's past cert decisions.
  if (socketClientCert) {
    cert.reset(socketClientCert->GetCert());
    if (!cert) {
      goto loser;
    }

    // Get the private key
    privKey.reset(PK11_FindKeyByAnyCert(cert.get(), wincx));
    if (!privKey) {
      goto loser;
    }

    *mPRetCert = cert.release();
    *mPRetKey = privKey.release();
    mRV = SECSuccess;
    return;
  }

  // create caNameStrings
  arena.reset(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    goto loser;
  }

  caNameStrings = static_cast<char**>(
    PORT_ArenaAlloc(arena.get(), sizeof(char*) * mCANames->nnames));
  if (!caNameStrings) {
    goto loser;
  }

  mRV = nsConvertCANamesToStrings(arena, caNameStrings, mCANames);
  if (mRV != SECSuccess) {
    goto loser;
  }

  // find valid user cert and key pair
  if (nsGetUserCertChoice() == UserCertChoice::Auto) {
    // automatically find the right cert

    // find all user certs that are valid and for SSL
    UniqueCERTCertList certList(
      CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), certUsageSSLClient,
                                false, true, wincx));
    if (!certList) {
      goto loser;
    }

    // filter the list to those issued by CAs supported by the server
    mRV = CERT_FilterCertListByCANames(certList.get(), mCANames->nnames,
                                       caNameStrings, certUsageSSLClient);
    if (mRV != SECSuccess) {
      goto loser;
    }

    // make sure the list is not empty
    if (CERT_LIST_END(CERT_LIST_HEAD(certList), certList)) {
      goto loser;
    }

    UniqueCERTCertificate lowPrioNonrepCert;

    // loop through the list until we find a cert with a key
    for (CERTCertListNode* node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         node = CERT_LIST_NEXT(node)) {
      // if the certificate has restriction and we do not satisfy it we do not
      // use it
      privKey.reset(PK11_FindKeyByAnyCert(node->cert, wincx));
      if (privKey) {
        if (hasExplicitKeyUsageNonRepudiation(node->cert)) {
          privKey = nullptr;
          // Not a preferred cert
          if (!lowPrioNonrepCert) { // did not yet find a low prio cert
            lowPrioNonrepCert.reset(CERT_DupCertificate(node->cert));
          }
        } else {
          // this is a good cert to present
          cert.reset(CERT_DupCertificate(node->cert));
          break;
        }
      }
      if (PR_GetError() == SEC_ERROR_BAD_PASSWORD) {
        // problem with password: bail
        goto loser;
      }
    }

    if (!cert && lowPrioNonrepCert) {
      cert = Move(lowPrioNonrepCert);
      privKey.reset(PK11_FindKeyByAnyCert(cert.get(), wincx));
    }

    if (!cert) {
      goto loser;
    }
  } else { // Not Auto => ask
    // Get the SSL Certificate

    const nsACString& hostname = mSocketInfo->GetHostName();

    RefPtr<nsClientAuthRememberService> cars =
      mSocketInfo->SharedState().GetClientAuthRememberService();

    bool hasRemembered = false;
    nsCString rememberedDBKey;
    if (cars) {
      bool found;
      rv = cars->HasRememberedDecision(hostname,
                                       mSocketInfo->GetOriginAttributes(),
                                       mServerCert, rememberedDBKey, &found);
      if (NS_SUCCEEDED(rv) && found) {
        hasRemembered = true;
      }
    }

    if (hasRemembered && !rememberedDBKey.IsEmpty()) {
      nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
      if (certdb) {
        nsCOMPtr<nsIX509Cert> foundCert;
        rv = certdb->FindCertByDBKey(rememberedDBKey, getter_AddRefs(foundCert));
        if (NS_SUCCEEDED(rv) && foundCert) {
          nsNSSCertificate* objCert =
            BitwiseCast<nsNSSCertificate*, nsIX509Cert*>(foundCert.get());
          if (objCert) {
            cert.reset(objCert->GetCert());
          }
        }

        if (!cert) {
          hasRemembered = false;
        }
      }
    }

    if (!hasRemembered) {
      // user selects a cert to present
      nsCOMPtr<nsIClientAuthDialogs> dialogs;

      // find all user certs that are for SSL
      // note that we are allowing expired certs in this list
      UniqueCERTCertList certList(
        CERT_FindUserCertsByUsage(CERT_GetDefaultCertDB(), certUsageSSLClient,
                                  false, false, wincx));
      if (!certList) {
        goto loser;
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
        goto loser;
      }

      UniquePORTString corg(CERT_GetOrgName(&mServerCert->subject));
      nsAutoCString org(corg.get());

      UniquePORTString cissuer(CERT_GetOrgName(&mServerCert->issuer));
      nsAutoCString issuer(cissuer.get());

      nsCOMPtr<nsIMutableArray> certArray = nsArrayBase::Create();
      if (!certArray) {
        goto loser;
      }

      for (CERTCertListNode* node = CERT_LIST_HEAD(certList);
           !CERT_LIST_END(node, certList);
           node = CERT_LIST_NEXT(node)) {
        nsCOMPtr<nsIX509Cert> tempCert = nsNSSCertificate::Create(node->cert);
        if (!tempCert) {
          goto loser;
        }

        rv = certArray->AppendElement(tempCert, false);
        if (NS_FAILED(rv)) {
          goto loser;
        }
      }

      // Throw up the client auth dialog and get back the index of the selected cert
      rv = getNSSDialogs(getter_AddRefs(dialogs),
                         NS_GET_IID(nsIClientAuthDialogs),
                         NS_CLIENTAUTHDIALOGS_CONTRACTID);

      if (NS_FAILED(rv)) {
        goto loser;
      }

      uint32_t selectedIndex = 0;
      bool certChosen = false;
      rv = dialogs->ChooseCertificate(mSocketInfo, hostname,
                                      mSocketInfo->GetPort(), org, issuer,
                                      certArray, &selectedIndex, &certChosen);
      if (NS_FAILED(rv)) {
        goto loser;
      }

      // even if the user has canceled, we want to remember that, to avoid repeating prompts
      bool wantRemember = false;
      mSocketInfo->GetRememberClientAuthCertificate(&wantRemember);

      if (certChosen) {
        nsCOMPtr<nsIX509Cert> selectedCert = do_QueryElementAt(certArray,
                                                               selectedIndex);
        if (!selectedCert) {
          goto loser;
        }
        cert.reset(selectedCert->GetCert());
      }

      if (cars && wantRemember) {
        cars->RememberDecision(hostname, mSocketInfo->GetOriginAttributes(),
                               mServerCert, certChosen ? cert.get() : nullptr);
      }
    }

    if (!cert) {
      goto loser;
    }

    // go get the private key
    privKey.reset(PK11_FindKeyByAnyCert(cert.get(), wincx));
    if (!privKey) {
      goto loser;
    }
  }
  goto done;

loser:
  if (mRV == SECSuccess) {
    mRV = SECFailure;
  }
done:
  int error = PR_GetError();

  *mPRetCert = cert.release();
  *mPRetKey = privKey.release();

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
    MOZ_ASSERT_UNREACHABLE("NSS: Error importing socket");
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

static const SSLSignatureScheme sEnabledSignatureSchemes[] = {
  ssl_sig_ecdsa_secp256r1_sha256,
  ssl_sig_ecdsa_secp384r1_sha384,
  ssl_sig_ecdsa_secp521r1_sha512,
  ssl_sig_rsa_pss_sha256,
  ssl_sig_rsa_pss_sha384,
  ssl_sig_rsa_pss_sha512,
  ssl_sig_rsa_pkcs1_sha256,
  ssl_sig_rsa_pkcs1_sha384,
  ssl_sig_rsa_pkcs1_sha512,
  ssl_sig_ecdsa_sha1,
  ssl_sig_rsa_pkcs1_sha1,
};

static nsresult
nsSSLIOLayerSetOptions(PRFileDesc* fd, bool forSTARTTLS,
                       bool haveProxy, const char* host, int32_t port,
                       nsNSSSocketInfo* infoObject)
{
  nsNSSShutDownPreventionLock locker;
  if (forSTARTTLS || haveProxy) {
    if (SECSuccess != SSL_OptionSet(fd, SSL_SECURITY, false)) {
      return NS_ERROR_FAILURE;
    }
  }

  SSLVersionRange range;
  if (SSL_VersionRangeGet(fd, &range) != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  if ((infoObject->GetProviderFlags() & nsISocketProvider::BE_CONSERVATIVE) &&
      (range.max > SSL_LIBRARY_VERSION_TLS_1_2)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] nsSSLIOLayerSetOptions: range.max limited to 1.2 due to BE_CONSERVATIVE flag\n",
             fd));
    range.max = SSL_LIBRARY_VERSION_TLS_1_2;
  }

  uint16_t maxEnabledVersion = range.max;
  infoObject->SharedState().IOLayerHelpers()
    .adjustForTLSIntolerance(infoObject->GetHostName(), infoObject->GetPort(),
                             range);
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
         ("[%p] nsSSLIOLayerSetOptions: using TLS version range (0x%04x,0x%04x)\n",
          fd, static_cast<unsigned int>(range.min),
              static_cast<unsigned int>(range.max)));

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

  if (range.max > SSL_LIBRARY_VERSION_TLS_1_2) {
    SSL_CipherPrefSet(fd, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, false);
    SSL_CipherPrefSet(fd, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, false);
    SSL_CipherPrefSet(fd, TLS_DHE_RSA_WITH_AES_128_CBC_SHA, false);
    SSL_CipherPrefSet(fd, TLS_DHE_RSA_WITH_AES_256_CBC_SHA, false);
  }

  // Include a modest set of named groups.
  const SSLNamedGroup namedGroups[] = {
    ssl_grp_ec_curve25519, ssl_grp_ec_secp256r1, ssl_grp_ec_secp384r1,
    ssl_grp_ec_secp521r1, ssl_grp_ffdhe_2048, ssl_grp_ffdhe_3072
  };
  if (SECSuccess != SSL_NamedGroupConfig(fd, namedGroups,
                                         mozilla::ArrayLength(namedGroups))) {
    return NS_ERROR_FAILURE;
  }
  // This ensures that we send key shares for X25519 and P-256 in TLS 1.3, so
  // that servers are less likely to use HelloRetryRequest.
  if (SECSuccess != SSL_SendAdditionalKeyShares(fd, 1)) {
    return NS_ERROR_FAILURE;
  }

  if (SECSuccess != SSL_SignatureSchemePrefSet(fd, sEnabledSignatureSchemes,
                      mozilla::ArrayLength(sEnabledSignatureSchemes))) {
    return NS_ERROR_FAILURE;
  }

  bool enabled = infoObject->SharedState().IsOCSPStaplingEnabled();
  if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_OCSP_STAPLING, enabled)) {
    return NS_ERROR_FAILURE;
  }

  bool sctsEnabled = infoObject->SharedState().IsSignedCertTimestampsEnabled();
  if (SECSuccess != SSL_OptionSet(fd, SSL_ENABLE_SIGNED_CERT_TIMESTAMPS,
      sctsEnabled)) {
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
  if (flags & nsISocketProvider::BE_CONSERVATIVE) {
    peerId.AppendLiteral("beConservative:");
  }
  peerId.Append(host);
  peerId.Append(':');
  peerId.AppendInt(port);
  nsAutoCString suffix;
  infoObject->GetOriginAttributes().CreateSuffix(suffix);
  peerId.Append(suffix);
  if (SECSuccess != SSL_SetSockPeerID(fd, peerId.get())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsSSLIOLayerAddToSocket(int32_t family,
                        const char* host,
                        int32_t port,
                        nsIProxyInfo* proxy,
                        const OriginAttributes& originAttributes,
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
  infoObject->SetOriginAttributes(originAttributes);

  bool haveProxy = false;
  if (proxy) {
    nsCString proxyHost;
    proxy->GetHost(proxyHost);
    haveProxy = !proxyHost.IsEmpty();
  }

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
    MOZ_ASSERT_UNREACHABLE("NSS: Error importing socket");
    goto loser;
  }

  infoObject->SetFileDescPtr(sslSock);

  rv = nsSSLIOLayerSetOptions(sslSock, forSTARTTLS, haveProxy, host, port,
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

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] Socket set up\n", (void*) sslSock));
  infoObject->QueryInterface(NS_GET_IID(nsISupports), (void**) (info));

  // We are going use a clear connection first //
  if (forSTARTTLS || haveProxy) {
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

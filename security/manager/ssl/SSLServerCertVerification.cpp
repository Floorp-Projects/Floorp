/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// For connections that are not processed on the socket transport thread, we do
// NOT use the async logic described below. Instead, we authenticate the
// certificate on the thread that the connection's I/O happens on,
// synchronously. This allows us to do certificate verification for blocking
// (not non-blocking) sockets and sockets that have their I/O processed on a
// thread other than the socket transport service thread. Also, we DO NOT
// support blocking sockets on the socket transport service thread at all.
//
// During certificate authentication, we call CERT_PKIXVerifyCert or
// CERT_VerifyCert. These functions may make zero or more HTTP requests
// for OCSP responses, CRLs, intermediate certificates, etc. Our fetching logic
// for these requests processes them on the socket transport service thread.
//
// If the connection for which we are verifying the certificate is happening
// on the socket transport thread (the usually case, at least for HTTP), then
// if our cert auth hook were to call the CERT_*Verify* functions directly,
// there would be a deadlock: The CERT_*Verify* function would cause an event
// to be asynchronously posted to the socket transport thread, and then it
// would block the socket transport thread waiting to be notified of the HTTP
// response. However, the HTTP request would never actually be processed
// because the socket transport thread would be blocked and so it wouldn't be
// able process HTTP requests. (i.e. Deadlock.)
//
// Consequently, when we are asked to verify a certificate on the socket
// transport service thread, we must always call the CERT_*Verify* cert
// functions on another thread. To accomplish this, our auth cert hook
// dispatches a SSLServerCertVerificationJob to a pool of background threads,
// and then immediately returns SECWouldBlock to libssl. These jobs are where
// the CERT_*Verify* functions are actually called.
//
// When our auth cert hook returns SECWouldBlock, libssl will carry on the
// handshake while we validate the certificate. This will free up the socket
// transport thread so that HTTP requests--in particular, the OCSP/CRL/cert
// requests needed for cert verification as mentioned above--can be processed.
//
// Once the CERT_*Verify* function returns, the cert verification job
// dispatches a SSLServerCertVerificationResult to the socket transport thread;
// the SSLServerCertVerificationResult will notify libssl that the certificate
// authentication is complete. Once libssl is notified that the authentication
// is complete, it will continue the SSL handshake (if it hasn't already
// finished) and it will begin allowing us to send/receive data on the
// connection.
//
// Timeline of events (for connections managed by the socket transport service):
//
//    * libssl calls SSLServerCertVerificationJob::Dispatch on the socket
//      transport thread.
//    * SSLServerCertVerificationJob::Dispatch queues a job
//      (instance of SSLServerCertVerificationJob) to its background thread
//      pool and returns.
//    * One of the background threads calls CERT_*Verify*, which may enqueue
//      some HTTP request(s) onto the socket transport thread, and then
//      blocks that background thread waiting for the responses and/or timeouts
//      or errors for those requests.
//    * Once those HTTP responses have all come back or failed, the
//      CERT_*Verify* function returns a result indicating that the validation
//      succeeded or failed.
//    * If the validation succeeded, then a SSLServerCertVerificationResult
//      event is posted to the socket transport thread, and the cert
//      verification thread becomes free to verify other certificates.
//    * Otherwise, a CertErrorRunnable is posted to the socket transport thread
//      and then to the main thread (blocking both, see CertErrorRunnable) to
//      do cert override processing and bad cert listener notification. Then
//      the cert verification thread becomes free to verify other certificates.
//    * After processing cert overrides, the CertErrorRunnable will dispatch a
//      SSLServerCertVerificationResult event to the socket transport thread to
//      notify it of the result of the override processing; then it returns,
//      freeing up the main thread.
//    * The SSLServerCertVerificationResult event will either wake up the
//      socket (using SSL_RestartHandshakeAfterServerCert) if validation
//      succeeded or there was an error override, or it will set an error flag
//      so that the next I/O operation on the socket will fail, causing the
//      socket transport thread to close the connection.
//
// Cert override processing must happen on the main thread because it accesses
// the nsICertOverrideService, and that service must be accessed on the main
// thread because some extensions (Selenium, in particular) replace it with a
// Javascript implementation, and chrome JS must always be run on the main
// thread.
//
// SSLServerCertVerificationResult must be dispatched to the socket transport
// thread because we must only call SSL_* functions on the socket transport
// thread since they may do I/O, because many parts of nsNSSSocketInfo (the
// subclass of TransportSecurityInfo used when validating certificates during
// an SSL handshake) and the PSM NSS I/O layer are not thread-safe, and because
// we need the event to interrupt the PR_Poll that may waiting for I/O on the
// socket for which we are validating the cert.

#include "SSLServerCertVerification.h"

#include <cstring>

#include "BRNameMatchingPolicy.h"
#include "CertVerifier.h"
#include "CryptoTask.h"
#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "PSMRunnable.h"
#include "RootCertificateTelemetryUtils.h"
#include "ScopedNSSTypes.h"
#include "SharedSSLState.h"
#include "cert.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/net/DNS.h"
#include "mozilla/unused.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIBadCertListener2.h"
#include "nsICertOverrideService.h"
#include "nsISiteSecurityService.h"
#include "nsISocketProvider.h"
#include "nsIThreadPool.h"
#include "nsNSSComponent.h"
#include "nsNSSIOLayer.h"
#include "nsNSSShutDown.h"
#include "nsServiceManagerUtils.h"
#include "nsURLHelper.h"
#include "nsXPCOMCIDInternal.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "secerr.h"
#include "secoidt.h"
#include "secport.h"
#include "ssl.h"
#include "sslerr.h"

extern mozilla::LazyLogModule gPIPNSSLog;

using namespace mozilla::pkix;

namespace mozilla { namespace psm {

namespace {

// do not use a nsCOMPtr to avoid static initializer/destructor
nsIThreadPool* gCertVerificationThreadPool = nullptr;

// We avoid using a mutex for the success case to avoid lock-related
// performance issues. However, we do use a lock in the error case to simplify
// the code, since performance in the error case is not important.
Mutex* gSSLVerificationTelemetryMutex = nullptr;

// We add a mutex to serialize PKCS11 database operations
Mutex* gSSLVerificationPK11Mutex = nullptr;

} // unnamed namespace

// Called when the socket transport thread starts, to initialize the SSL cert
// verification thread pool. By tying the thread pool startup/shutdown directly
// to the STS thread's lifetime, we ensure that they are *always* available for
// SSL connections and that there are no races during startup and especially
// shutdown. (Previously, we have had multiple problems with races in PSM
// background threads, and the race-prevention/shutdown logic used there is
// brittle. Since this service is critical to things like downloading updates,
// we take no chances.) Also, by doing things this way, we avoid the need for
// locks, since gCertVerificationThreadPool is only ever accessed on the socket
// transport thread.
void
InitializeSSLServerCertVerificationThreads()
{
  gSSLVerificationTelemetryMutex = new Mutex("SSLVerificationTelemetryMutex");
  gSSLVerificationPK11Mutex = new Mutex("SSLVerificationPK11Mutex");
  // TODO: tuning, make parameters preferences
  // XXX: instantiate nsThreadPool directly, to make this more bulletproof.
  // Currently, the nsThreadPool.h header isn't exported for us to do so.
  nsresult rv = CallCreateInstance(NS_THREADPOOL_CONTRACTID,
                                   &gCertVerificationThreadPool);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create SSL cert verification threads.");
    return;
  }

  (void) gCertVerificationThreadPool->SetIdleThreadLimit(5);
  (void) gCertVerificationThreadPool->SetIdleThreadTimeout(30 * 1000);
  (void) gCertVerificationThreadPool->SetThreadLimit(5);
  (void) gCertVerificationThreadPool->SetName(NS_LITERAL_CSTRING("SSL Cert"));
}

// Called when the socket transport thread finishes, to destroy the thread
// pool. Since the socket transport service has stopped processing events, it
// will not attempt any more SSL I/O operations, so it is clearly safe to shut
// down the SSL cert verification infrastructure. Also, the STS will not
// dispatch many SSL verification result events at this point, so any pending
// cert verifications will (correctly) fail at the point they are dispatched.
//
// The other shutdown race condition that is possible is a race condition with
// shutdown of the nsNSSComponent service. We use the
// nsNSSShutdownPreventionLock where needed (not here) to prevent that.
void StopSSLServerCertVerificationThreads()
{
  if (gCertVerificationThreadPool) {
    gCertVerificationThreadPool->Shutdown();
    NS_RELEASE(gCertVerificationThreadPool);
  }
  if (gSSLVerificationTelemetryMutex) {
    delete gSSLVerificationTelemetryMutex;
    gSSLVerificationTelemetryMutex = nullptr;
  }
  if (gSSLVerificationPK11Mutex) {
    delete gSSLVerificationPK11Mutex;
    gSSLVerificationPK11Mutex = nullptr;
  }
}

namespace {

void
LogInvalidCertError(nsNSSSocketInfo* socketInfo,
                    PRErrorCode errorCode,
                    ::mozilla::psm::SSLErrorMessageType errorMessageType)
{
  nsString message;
  socketInfo->GetErrorLogMessage(errorCode, errorMessageType, message);
  if (!message.IsEmpty()) {
    nsContentUtils::LogSimpleConsoleError(message, "SSL");
  }
}

// Dispatched to the STS thread to notify the infoObject of the verification
// result.
//
// This will cause the PR_Poll in the STS thread to return, so things work
// correctly even if the STS thread is blocked polling (only) on the file
// descriptor that is waiting for this result.
class SSLServerCertVerificationResult : public Runnable
{
public:
  NS_DECL_NSIRUNNABLE

  SSLServerCertVerificationResult(nsNSSSocketInfo* infoObject,
                                  PRErrorCode errorCode,
                                  Telemetry::ID telemetryID = Telemetry::HistogramCount,
                                  uint32_t telemetryValue = -1,
                                  SSLErrorMessageType errorMessageType =
                                      PlainErrorMessage);

  void Dispatch();
private:
  const RefPtr<nsNSSSocketInfo> mInfoObject;
public:
  const PRErrorCode mErrorCode;
  const SSLErrorMessageType mErrorMessageType;
  const Telemetry::ID mTelemetryID;
  const uint32_t mTelemetryValue;
};

class CertErrorRunnable : public SyncRunnableBase
{
 public:
  CertErrorRunnable(const void* fdForLogging,
                    nsIX509Cert* cert,
                    nsNSSSocketInfo* infoObject,
                    PRErrorCode defaultErrorCodeToReport,
                    uint32_t collectedErrors,
                    PRErrorCode errorCodeTrust,
                    PRErrorCode errorCodeMismatch,
                    PRErrorCode errorCodeTime,
                    uint32_t providerFlags)
    : mFdForLogging(fdForLogging), mCert(cert), mInfoObject(infoObject),
      mDefaultErrorCodeToReport(defaultErrorCodeToReport),
      mCollectedErrors(collectedErrors),
      mErrorCodeTrust(errorCodeTrust),
      mErrorCodeMismatch(errorCodeMismatch),
      mErrorCodeTime(errorCodeTime),
      mProviderFlags(providerFlags)
  {
  }

  virtual void RunOnTargetThread();
  RefPtr<SSLServerCertVerificationResult> mResult; // out
private:
  SSLServerCertVerificationResult* CheckCertOverrides();

  const void* const mFdForLogging; // may become an invalid pointer; do not dereference
  const nsCOMPtr<nsIX509Cert> mCert;
  const RefPtr<nsNSSSocketInfo> mInfoObject;
  const PRErrorCode mDefaultErrorCodeToReport;
  const uint32_t mCollectedErrors;
  const PRErrorCode mErrorCodeTrust;
  const PRErrorCode mErrorCodeMismatch;
  const PRErrorCode mErrorCodeTime;
  const uint32_t mProviderFlags;
};

// A probe value of 1 means "no error".
uint32_t
MapOverridableErrorToProbeValue(PRErrorCode errorCode)
{
  switch (errorCode)
  {
    case SEC_ERROR_UNKNOWN_ISSUER:                     return  2;
    case SEC_ERROR_CA_CERT_INVALID:                    return  3;
    case SEC_ERROR_UNTRUSTED_ISSUER:                   return  4;
    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:         return  5;
    case SEC_ERROR_UNTRUSTED_CERT:                     return  6;
    case SEC_ERROR_INADEQUATE_KEY_USAGE:               return  7;
    case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:  return  8;
    case SSL_ERROR_BAD_CERT_DOMAIN:                    return  9;
    case SEC_ERROR_EXPIRED_CERTIFICATE:                return 10;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY: return 11;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA: return 12;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE: return 13;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE: return 14;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE:
      return 15;
    case SEC_ERROR_INVALID_TIME: return 16;
    case mozilla::pkix::MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME: return 17;
  }
  NS_WARNING("Unknown certificate error code. Does MapOverridableErrorToProbeValue "
             "handle everything in DetermineCertOverrideErrors?");
  return 0;
}

static uint32_t
MapCertErrorToProbeValue(PRErrorCode errorCode)
{
  uint32_t probeValue;
  switch (errorCode)
  {
    // see security/pkix/include/pkix/Result.h
#define MOZILLA_PKIX_MAP(name, value, nss_name) case nss_name: probeValue = value; break;
    MOZILLA_PKIX_MAP_LIST
#undef MOZILLA_PKIX_MAP
    default: return 0;
  }

  // Since FATAL_ERROR_FLAG is 0x800, fatal error values are much larger than
  // non-fatal error values. To conserve space, we remap these so they start at
  // (decimal) 90 instead of 0x800. Currently there are ~50 non-fatal errors
  // mozilla::pkix might return, so saving space for 90 should be sufficient
  // (similarly, there are 4 fatal errors, so saving space for 10 should also
  // be sufficient).
  static_assert(FATAL_ERROR_FLAG == 0x800,
                "mozilla::pkix::FATAL_ERROR_FLAG is not what we were expecting");
  if (probeValue & FATAL_ERROR_FLAG) {
    probeValue ^= FATAL_ERROR_FLAG;
    probeValue += 90;
  }
  return probeValue;
}

SECStatus
DetermineCertOverrideErrors(const UniqueCERTCertificate& cert,
                            const char* hostName,
                            PRTime now, PRErrorCode defaultErrorCodeToReport,
                            /*out*/ uint32_t& collectedErrors,
                            /*out*/ PRErrorCode& errorCodeTrust,
                            /*out*/ PRErrorCode& errorCodeMismatch,
                            /*out*/ PRErrorCode& errorCodeTime)
{
  MOZ_ASSERT(cert);
  MOZ_ASSERT(hostName);
  MOZ_ASSERT(collectedErrors == 0);
  MOZ_ASSERT(errorCodeTrust == 0);
  MOZ_ASSERT(errorCodeMismatch == 0);
  MOZ_ASSERT(errorCodeTime == 0);

  // Assumes the error prioritization described in mozilla::pkix's
  // BuildForward function. Also assumes that CheckCertHostname was only
  // called if CertVerifier::VerifyCert succeeded.
  switch (defaultErrorCodeToReport) {
    case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME:
    {
      collectedErrors = nsICertOverrideService::ERROR_UNTRUSTED;
      errorCodeTrust = defaultErrorCodeToReport;

      SECCertTimeValidity validity = CERT_CheckCertValidTimes(cert.get(), now,
                                                              false);
      if (validity == secCertTimeUndetermined) {
        // This only happens if cert is null. CERT_CheckCertValidTimes will
        // have set the error code to SEC_ERROR_INVALID_ARGS. We should really
        // be using mozilla::pkix here anyway.
        MOZ_ASSERT(PR_GetError() == SEC_ERROR_INVALID_ARGS);
        return SECFailure;
      }
      if (validity == secCertTimeExpired) {
        collectedErrors |= nsICertOverrideService::ERROR_TIME;
        errorCodeTime = SEC_ERROR_EXPIRED_CERTIFICATE;
      } else if (validity == secCertTimeNotValidYet) {
        collectedErrors |= nsICertOverrideService::ERROR_TIME;
        errorCodeTime =
          mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE;
      }
      break;
    }

    case SEC_ERROR_INVALID_TIME:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE:
      collectedErrors = nsICertOverrideService::ERROR_TIME;
      errorCodeTime = defaultErrorCodeToReport;
      break;

    case SSL_ERROR_BAD_CERT_DOMAIN:
      collectedErrors = nsICertOverrideService::ERROR_MISMATCH;
      errorCodeMismatch = SSL_ERROR_BAD_CERT_DOMAIN;
      break;

    case 0:
      NS_ERROR("No error code set during certificate validation failure.");
      PR_SetError(PR_INVALID_STATE_ERROR, 0);
      return SECFailure;

    default:
      PR_SetError(defaultErrorCodeToReport, 0);
      return SECFailure;
  }

  if (defaultErrorCodeToReport != SSL_ERROR_BAD_CERT_DOMAIN) {
    Input certInput;
    if (certInput.Init(cert->derCert.data, cert->derCert.len) != Success) {
      PR_SetError(SEC_ERROR_BAD_DER, 0);
      return SECFailure;
    }
    Input hostnameInput;
    Result result = hostnameInput.Init(uint8_t_ptr_cast(hostName),
                                       strlen(hostName));
    if (result != Success) {
      PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
      return SECFailure;
    }
    // Use a lax policy so as to not generate potentially spurious name
    // mismatch "hints".
    BRNameMatchingPolicy nameMatchingPolicy(
      BRNameMatchingPolicy::Mode::DoNotEnforce);
    // CheckCertHostname expects that its input represents a certificate that
    // has already been successfully validated by BuildCertChain. This is
    // obviously not the case, however, because we're in the error path of
    // certificate verification. Thus, this is problematic. In the future, it
    // would be nice to remove this optimistic additional error checking and
    // simply punt to the front-end, which can more easily (and safely) perform
    // extra checks to give the user hints as to why verification failed.
    result = CheckCertHostname(certInput, hostnameInput, nameMatchingPolicy);
    // Treat malformed name information as a domain mismatch.
    if (result == Result::ERROR_BAD_DER ||
        result == Result::ERROR_BAD_CERT_DOMAIN) {
      collectedErrors |= nsICertOverrideService::ERROR_MISMATCH;
      errorCodeMismatch = SSL_ERROR_BAD_CERT_DOMAIN;
    } else if (IsFatalError(result)) {
      // Because its input has not been validated by BuildCertChain,
      // CheckCertHostname can return an error that is less important than the
      // original certificate verification error. Only return an error result
      // from this function if we've encountered a fatal error.
      PR_SetError(MapResultToPRErrorCode(result), 0);
      return SECFailure;
    }
  }

  return SECSuccess;
}

SSLServerCertVerificationResult*
CertErrorRunnable::CheckCertOverrides()
{
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p][%p] top of CheckCertOverrides\n",
                                    mFdForLogging, this));
  // "Use" mFdForLogging in non-PR_LOGGING builds, too, to suppress
  // clang's -Wunused-private-field build warning for this variable:
  Unused << mFdForLogging;

  if (!NS_IsMainThread()) {
    NS_ERROR("CertErrorRunnable::CheckCertOverrides called off main thread");
    return new SSLServerCertVerificationResult(mInfoObject,
                                               mDefaultErrorCodeToReport);
  }

  int32_t port;
  mInfoObject->GetPort(&port);

  nsAutoCString hostWithPortString(mInfoObject->GetHostName());
  hostWithPortString.Append(':');
  hostWithPortString.AppendInt(port);

  uint32_t remaining_display_errors = mCollectedErrors;


  // If this is an HTTP Strict Transport Security host or a pinned host and the
  // certificate is bad, don't allow overrides (RFC 6797 section 12.1,
  // HPKP draft spec section 2.6).
  bool strictTransportSecurityEnabled = false;
  bool hasPinningInformation = false;
  nsCOMPtr<nsISiteSecurityService> sss(do_GetService(NS_SSSERVICE_CONTRACTID));
  if (!sss) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("[%p][%p] couldn't get nsISiteSecurityService to check for HSTS/HPKP\n",
            mFdForLogging, this));
    return new SSLServerCertVerificationResult(mInfoObject,
                                               mDefaultErrorCodeToReport);
  }
  nsresult nsrv = sss->IsSecureHost(nsISiteSecurityService::HEADER_HSTS,
                                    mInfoObject->GetHostNameRaw(),
                                    mProviderFlags,
                                    &strictTransportSecurityEnabled);
  if (NS_FAILED(nsrv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("[%p][%p] checking for HSTS failed\n", mFdForLogging, this));
    return new SSLServerCertVerificationResult(mInfoObject,
                                               mDefaultErrorCodeToReport);
  }
  nsrv = sss->IsSecureHost(nsISiteSecurityService::HEADER_HPKP,
                           mInfoObject->GetHostNameRaw(),
                           mProviderFlags,
                           &hasPinningInformation);
  if (NS_FAILED(nsrv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("[%p][%p] checking for HPKP failed\n", mFdForLogging, this));
    return new SSLServerCertVerificationResult(mInfoObject,
                                               mDefaultErrorCodeToReport);
  }

  if (!strictTransportSecurityEnabled && !hasPinningInformation) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("[%p][%p] no HSTS or HPKP - overrides allowed\n",
            mFdForLogging, this));
    nsCOMPtr<nsICertOverrideService> overrideService =
      do_GetService(NS_CERTOVERRIDE_CONTRACTID);
    // it is fine to continue without the nsICertOverrideService

    uint32_t overrideBits = 0;

    if (overrideService)
    {
      bool haveOverride;
      bool isTemporaryOverride; // we don't care
      const nsACString& hostString(mInfoObject->GetHostName());
      nsrv = overrideService->HasMatchingOverride(hostString, port,
                                                  mCert,
                                                  &overrideBits,
                                                  &isTemporaryOverride,
                                                  &haveOverride);
      if (NS_SUCCEEDED(nsrv) && haveOverride)
      {
       // remove the errors that are already overriden
        remaining_display_errors &= ~overrideBits;
      }
    }

    if (!remaining_display_errors) {
      // This can double- or triple-count one certificate with multiple
      // different types of errors. Since this is telemetry and we just
      // want a ballpark answer, we don't care.
      if (mErrorCodeTrust != 0) {
        uint32_t probeValue = MapOverridableErrorToProbeValue(mErrorCodeTrust);
        Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, probeValue);
      }
      if (mErrorCodeMismatch != 0) {
        uint32_t probeValue = MapOverridableErrorToProbeValue(mErrorCodeMismatch);
        Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, probeValue);
      }
      if (mErrorCodeTime != 0) {
        uint32_t probeValue = MapOverridableErrorToProbeValue(mErrorCodeTime);
        Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, probeValue);
      }

      // all errors are covered by override rules, so let's accept the cert
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
             ("[%p][%p] All errors covered by override rules\n",
             mFdForLogging, this));
      return new SSLServerCertVerificationResult(mInfoObject, 0);
    }
  } else {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("[%p][%p] HSTS or HPKP - no overrides allowed\n",
            mFdForLogging, this));
  }

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
         ("[%p][%p] Certificate error was not overridden\n",
         mFdForLogging, this));

  // Ok, this is a full stop.
  // First, deliver the technical details of the broken SSL status.

  // Try to get a nsIBadCertListener2 implementation from the socket consumer.
  nsCOMPtr<nsISSLSocketControl> sslSocketControl = do_QueryInterface(
    NS_ISUPPORTS_CAST(nsITransportSecurityInfo*, mInfoObject));
  if (sslSocketControl) {
    nsCOMPtr<nsIInterfaceRequestor> cb;
    sslSocketControl->GetNotificationCallbacks(getter_AddRefs(cb));
    if (cb) {
      nsCOMPtr<nsIBadCertListener2> bcl = do_GetInterface(cb);
      if (bcl) {
        nsIInterfaceRequestor* csi
          = static_cast<nsIInterfaceRequestor*>(mInfoObject);
        bool suppressMessage = false; // obsolete, ignored
        nsrv = bcl->NotifyCertProblem(csi, mInfoObject->SSLStatus(),
                                      hostWithPortString, &suppressMessage);
      }
    }
  }

  // pick the error code to report by priority
  PRErrorCode errorCodeToReport = mErrorCodeTrust    ? mErrorCodeTrust
                                : mErrorCodeMismatch ? mErrorCodeMismatch
                                : mErrorCodeTime     ? mErrorCodeTime
                                : mDefaultErrorCodeToReport;

  SSLServerCertVerificationResult* result =
    new SSLServerCertVerificationResult(mInfoObject,
                                        errorCodeToReport,
                                        Telemetry::HistogramCount,
                                        -1,
                                        OverridableCertErrorMessage);

  LogInvalidCertError(mInfoObject,
                      result->mErrorCode,
                      result->mErrorMessageType);

  return result;
}

void
CertErrorRunnable::RunOnTargetThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  mResult = CheckCertOverrides();

  MOZ_ASSERT(mResult);
}

// Returns null with the error code (PR_GetError()) set if it does not create
// the CertErrorRunnable.
CertErrorRunnable*
CreateCertErrorRunnable(CertVerifier& certVerifier,
                        PRErrorCode defaultErrorCodeToReport,
                        nsNSSSocketInfo* infoObject,
                        const UniqueCERTCertificate& cert,
                        const void* fdForLogging,
                        uint32_t providerFlags,
                        PRTime now)
{
  MOZ_ASSERT(infoObject);
  MOZ_ASSERT(cert);

  uint32_t probeValue = MapCertErrorToProbeValue(defaultErrorCodeToReport);
  Telemetry::Accumulate(Telemetry::SSL_CERT_VERIFICATION_ERRORS, probeValue);

  uint32_t collected_errors = 0;
  PRErrorCode errorCodeTrust = 0;
  PRErrorCode errorCodeMismatch = 0;
  PRErrorCode errorCodeTime = 0;
  if (DetermineCertOverrideErrors(cert, infoObject->GetHostNameRaw(), now,
                                  defaultErrorCodeToReport, collected_errors,
                                  errorCodeTrust, errorCodeMismatch,
                                  errorCodeTime) != SECSuccess) {
    // Attempt to enforce that if DetermineCertOverrideErrors failed,
    // PR_SetError was set with a non-overridable error. This is because if we
    // return from CreateCertErrorRunnable without calling
    // infoObject->SetStatusErrorBits, we won't have the required information
    // to actually add a certificate error override. This results in a broken
    // UI which is annoying but not a security disaster.
    MOZ_ASSERT(!ErrorIsOverridable(PR_GetError()));
    return nullptr;
  }

  RefPtr<nsNSSCertificate> nssCert(nsNSSCertificate::Create(cert.get()));
  if (!nssCert) {
    NS_ERROR("nsNSSCertificate::Create failed");
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return nullptr;
  }

  if (!collected_errors) {
    // This will happen when CERT_*Verify* only returned error(s) that are
    // not on our whitelist of overridable certificate errors.
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug, ("[%p] !collected_errors: %d\n",
           fdForLogging, static_cast<int>(defaultErrorCodeToReport)));
    PR_SetError(defaultErrorCodeToReport, 0);
    return nullptr;
  }

  infoObject->SetStatusErrorBits(nssCert, collected_errors);

  return new CertErrorRunnable(fdForLogging,
                               static_cast<nsIX509Cert*>(nssCert.get()),
                               infoObject, defaultErrorCodeToReport,
                               collected_errors, errorCodeTrust,
                               errorCodeMismatch, errorCodeTime,
                               providerFlags);
}

// When doing async cert processing, we dispatch one of these runnables to the
// socket transport service thread, which blocks the socket transport
// service thread while it waits for the inner CertErrorRunnable to execute
// CheckCertOverrides on the main thread. CheckCertOverrides must block events
// on both of these threads because it calls TransportSecurityInfo::GetInterface(),
// which may call nsHttpConnection::GetInterface() through
// TransportSecurityInfo::mCallbacks. nsHttpConnection::GetInterface must always
// execute on the main thread, with the socket transport service thread
// blocked.
class CertErrorRunnableRunnable : public Runnable
{
public:
  explicit CertErrorRunnableRunnable(CertErrorRunnable* certErrorRunnable)
    : mCertErrorRunnable(certErrorRunnable)
  {
  }
private:
  NS_IMETHOD Run() override
  {
    nsresult rv = mCertErrorRunnable->DispatchToMainThreadAndWait();
    // The result must run on the socket transport thread, which we are already
    // on, so we can just run it directly, instead of dispatching it.
    if (NS_SUCCEEDED(rv)) {
      rv = mCertErrorRunnable->mResult ? mCertErrorRunnable->mResult->Run()
                                       : NS_ERROR_UNEXPECTED;
    }
    return rv;
  }
  RefPtr<CertErrorRunnable> mCertErrorRunnable;
};

class SSLServerCertVerificationJob : public Runnable
{
public:
  // Must be called only on the socket transport thread
  static SECStatus Dispatch(const RefPtr<SharedCertVerifier>& certVerifier,
                            const void* fdForLogging,
                            nsNSSSocketInfo* infoObject,
                            const UniqueCERTCertificate& serverCert,
                            const UniqueCERTCertList& peerCertChain,
                            SECItem* stapledOCSPResponse,
                            uint32_t providerFlags,
                            Time time,
                            PRTime prtime);
private:
  NS_DECL_NSIRUNNABLE

  // Must be called only on the socket transport thread
  SSLServerCertVerificationJob(const RefPtr<SharedCertVerifier>& certVerifier,
                               const void* fdForLogging,
                               nsNSSSocketInfo* infoObject,
                               const UniqueCERTCertificate& cert,
                               UniqueCERTCertList peerCertChain,
                               SECItem* stapledOCSPResponse,
                               uint32_t providerFlags,
                               Time time,
                               PRTime prtime);
  const RefPtr<SharedCertVerifier> mCertVerifier;
  const void* const mFdForLogging;
  const RefPtr<nsNSSSocketInfo> mInfoObject;
  const UniqueCERTCertificate mCert;
  UniqueCERTCertList mPeerCertChain;
  const uint32_t mProviderFlags;
  const Time mTime;
  const PRTime mPRTime;
  const TimeStamp mJobStartTime;
  const UniqueSECItem mStapledOCSPResponse;
};

SSLServerCertVerificationJob::SSLServerCertVerificationJob(
    const RefPtr<SharedCertVerifier>& certVerifier, const void* fdForLogging,
    nsNSSSocketInfo* infoObject, const UniqueCERTCertificate& cert,
    UniqueCERTCertList peerCertChain, SECItem* stapledOCSPResponse,
    uint32_t providerFlags, Time time, PRTime prtime)
  : mCertVerifier(certVerifier)
  , mFdForLogging(fdForLogging)
  , mInfoObject(infoObject)
  , mCert(CERT_DupCertificate(cert.get()))
  , mPeerCertChain(Move(peerCertChain))
  , mProviderFlags(providerFlags)
  , mTime(time)
  , mPRTime(prtime)
  , mJobStartTime(TimeStamp::Now())
  , mStapledOCSPResponse(SECITEM_DupItem(stapledOCSPResponse))
{
}

// This function assumes that we will only use the SPDY connection coalescing
// feature on connections where we have negotiated SPDY using NPN. If we ever
// talk SPDY without having negotiated it with SPDY, this code will give wrong
// and perhaps unsafe results.
//
// Returns SECSuccess on the initial handshake of all connections, on
// renegotiations for any connections where we did not negotiate SPDY, or on any
// SPDY connection where the server's certificate did not change.
//
// Prohibit changing the server cert only if we negotiated SPDY,
// in order to support SPDY's cross-origin connection pooling.
static SECStatus
BlockServerCertChangeForSpdy(nsNSSSocketInfo* infoObject,
                             const UniqueCERTCertificate& serverCert)
{
  // Get the existing cert. If there isn't one, then there is
  // no cert change to worry about.
  nsCOMPtr<nsIX509Cert> cert;

  RefPtr<nsSSLStatus> status(infoObject->SSLStatus());
  if (!status) {
    // If we didn't have a status, then this is the
    // first handshake on this connection, not a
    // renegotiation.
    return SECSuccess;
  }

  status->GetServerCert(getter_AddRefs(cert));
  if (!cert) {
    NS_NOTREACHED("every nsSSLStatus must have a cert"
                  "that implements nsIX509Cert");
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  // Filter out sockets that did not neogtiate SPDY via NPN
  nsAutoCString negotiatedNPN;
  nsresult rv = infoObject->GetNegotiatedNPN(negotiatedNPN);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "GetNegotiatedNPN() failed during renegotiation");

  if (NS_SUCCEEDED(rv) && !StringBeginsWith(negotiatedNPN,
                                            NS_LITERAL_CSTRING("spdy/"))) {
    return SECSuccess;
  }
  // If GetNegotiatedNPN() failed we will assume spdy for safety's safe
  if (NS_FAILED(rv)) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("BlockServerCertChangeForSpdy failed GetNegotiatedNPN() call."
            " Assuming spdy.\n"));
  }

  // Check to see if the cert has actually changed
  UniqueCERTCertificate c(cert->GetCert());
  NS_ASSERTION(c, "very bad and hopefully impossible state");
  bool sameCert = CERT_CompareCerts(c.get(), serverCert.get());
  if (sameCert) {
    return SECSuccess;
  }

  // Report an error - changed cert is confirmed
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
         ("SPDY Refused to allow new cert during renegotiation\n"));
  PR_SetError(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED, 0);
  return SECFailure;
}

void
AccumulateSubjectCommonNameTelemetry(const char* commonName,
                                     bool commonNameInSubjectAltNames)
{
  if (!commonName) {
    // 1 means no common name present
    Telemetry::Accumulate(Telemetry::BR_9_2_2_SUBJECT_COMMON_NAME, 1);
  } else if (!commonNameInSubjectAltNames) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("BR telemetry: common name '%s' not in subject alt. names "
            "(or the subject alt. names extension is not present)\n",
            commonName));
    // 2 means the common name is not present in subject alt names
    Telemetry::Accumulate(Telemetry::BR_9_2_2_SUBJECT_COMMON_NAME, 2);
  } else {
    // 0 means the common name is present in subject alt names
    Telemetry::Accumulate(Telemetry::BR_9_2_2_SUBJECT_COMMON_NAME, 0);
  }
}

// Returns true if and only if commonName ends with altName (minus its leading
// "*"). altName has already been checked to be of the form "*.<something>".
// commonName may be NULL.
static bool
TryMatchingWildcardSubjectAltName(const char* commonName,
                                  const nsACString& altName)
{
  return commonName &&
         StringEndsWith(nsDependentCString(commonName), Substring(altName, 1));
}

// Gathers telemetry on Baseline Requirements 9.2.1 (Subject Alternative
// Names Extension) and 9.2.2 (Subject Common Name Field).
// Specifically:
//  - whether or not the subject common name field is present
//  - whether or not the subject alternative names extension is present
//  - if there is a malformed entry in the subject alt. names extension
//  - if there is an entry in the subject alt. names extension corresponding
//    to the subject common name
// Telemetry is only gathered for certificates that chain to a trusted root
// in Mozilla's Root CA program.
// certList consists of a validated certificate chain. The end-entity
// certificate is first and the root (trust anchor) is last.
void
GatherBaselineRequirementsTelemetry(const UniqueCERTCertList& certList)
{
  CERTCertListNode* endEntityNode = CERT_LIST_HEAD(certList);
  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  PR_ASSERT(!(CERT_LIST_END(endEntityNode, certList) ||
              CERT_LIST_END(rootNode, certList)));
  if (CERT_LIST_END(endEntityNode, certList) ||
      CERT_LIST_END(rootNode, certList)) {
    return;
  }
  CERTCertificate* cert = endEntityNode->cert;
  PR_ASSERT(cert);
  if (!cert) {
    return;
  }
  UniquePORTString commonName(CERT_GetCommonName(&cert->subject));
  // This only applies to certificates issued by authorities in our root
  // program.
  CERTCertificate* rootCert = rootNode->cert;
  PR_ASSERT(rootCert);
  if (!rootCert) {
    return;
  }
  bool isBuiltIn = false;
  Result result = IsCertBuiltInRoot(rootCert, isBuiltIn);
  if (result != Success || !isBuiltIn) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("BR telemetry: root certificate for '%s' is not a built-in root "
            "(or IsCertBuiltInRoot failed)\n", commonName.get()));
    return;
  }
  ScopedAutoSECItem altNameExtension;
  SECStatus rv = CERT_FindCertExtension(cert, SEC_OID_X509_SUBJECT_ALT_NAME,
                                        &altNameExtension);
  if (rv != SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("BR telemetry: no subject alt names extension for '%s'\n",
            commonName.get()));
    // 1 means there is no subject alt names extension
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 1);
    AccumulateSubjectCommonNameTelemetry(commonName.get(), false);
    return;
  }

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  CERTGeneralName* subjectAltNames =
    CERT_DecodeAltNameExtension(arena.get(), &altNameExtension);
  if (!subjectAltNames) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
           ("BR telemetry: could not decode subject alt names for '%s'\n",
            commonName.get()));
    // 2 means the subject alt names extension could not be decoded
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 2);
    AccumulateSubjectCommonNameTelemetry(commonName.get(), false);
    return;
  }

  CERTGeneralName* currentName = subjectAltNames;
  bool commonNameInSubjectAltNames = false;
  bool nonDNSNameOrIPAddressPresent = false;
  bool malformedDNSNameOrIPAddressPresent = false;
  bool nonFQDNPresent = false;
  do {
    nsAutoCString altName;
    if (currentName->type == certDNSName) {
      altName.Assign(BitwiseCast<char*, unsigned char*>(
                       currentName->name.other.data),
                     currentName->name.other.len);
      nsDependentCString altNameWithoutWildcard(altName, 0);
      if (StringBeginsWith(altNameWithoutWildcard, NS_LITERAL_CSTRING("*."))) {
        altNameWithoutWildcard.Rebind(altName, 2);
        commonNameInSubjectAltNames |=
          TryMatchingWildcardSubjectAltName(commonName.get(), altName);
      }
      // net_IsValidHostName appears to return true for valid IP addresses,
      // which would be invalid for a DNS name.
      // Note that the net_IsValidHostName check will catch things like
      // "a.*.example.com".
      if (!net_IsValidHostName(altNameWithoutWildcard) ||
          net_IsValidIPv4Addr(altName.get(), altName.Length()) ||
          net_IsValidIPv6Addr(altName.get(), altName.Length())) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
               ("BR telemetry: DNSName '%s' not valid (for '%s')\n",
                altName.get(), commonName.get()));
        malformedDNSNameOrIPAddressPresent = true;
      }
      if (!altName.Contains('.')) {
        nonFQDNPresent = true;
      }
    } else if (currentName->type == certIPAddress) {
      // According to DNS.h, this includes space for the null-terminator
      char buf[net::kNetAddrMaxCStrBufSize] = { 0 };
      PRNetAddr addr;
      if (currentName->name.other.len == 4) {
        addr.inet.family = PR_AF_INET;
        memcpy(&addr.inet.ip, currentName->name.other.data,
               currentName->name.other.len);
        if (PR_NetAddrToString(&addr, buf, sizeof(buf) - 1) != PR_SUCCESS) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
               ("BR telemetry: IPAddress (v4) not valid (for '%s')\n",
                commonName.get()));
          malformedDNSNameOrIPAddressPresent = true;
        } else {
          altName.Assign(buf);
        }
      } else if (currentName->name.other.len == 16) {
        addr.inet.family = PR_AF_INET6;
        memcpy(&addr.ipv6.ip, currentName->name.other.data,
               currentName->name.other.len);
        if (PR_NetAddrToString(&addr, buf, sizeof(buf) - 1) != PR_SUCCESS) {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
               ("BR telemetry: IPAddress (v6) not valid (for '%s')\n",
                commonName.get()));
          malformedDNSNameOrIPAddressPresent = true;
        } else {
          altName.Assign(buf);
        }
      } else {
        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
               ("BR telemetry: IPAddress not valid (for '%s')\n",
                commonName.get()));
        malformedDNSNameOrIPAddressPresent = true;
      }
    } else {
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
             ("BR telemetry: non-DNSName, non-IPAddress present for '%s'\n",
              commonName.get()));
      nonDNSNameOrIPAddressPresent = true;
    }
    if (commonName && altName.Equals(commonName.get())) {
      commonNameInSubjectAltNames = true;
    }
    currentName = CERT_GetNextGeneralName(currentName);
  } while (currentName && currentName != subjectAltNames);

  if (nonDNSNameOrIPAddressPresent) {
    // 3 means there's an entry that isn't an ip address or dns name
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 3);
  }
  if (malformedDNSNameOrIPAddressPresent) {
    // 4 means there's a malformed ip address or dns name entry
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 4);
  }
  if (nonFQDNPresent) {
    // 5 means there's a DNS name entry with a non-fully-qualified domain name
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 5);
  }
  if (!nonDNSNameOrIPAddressPresent && !malformedDNSNameOrIPAddressPresent &&
      !nonFQDNPresent) {
    // 0 means the extension is acceptable
    Telemetry::Accumulate(Telemetry::BR_9_2_1_SUBJECT_ALT_NAMES, 0);
  }

  AccumulateSubjectCommonNameTelemetry(commonName.get(),
                                       commonNameInSubjectAltNames);
}

// Gather telemetry on whether the end-entity cert for a server has the
// required TLS Server Authentication EKU, or any others
void
GatherEKUTelemetry(const UniqueCERTCertList& certList)
{
  CERTCertListNode* endEntityNode = CERT_LIST_HEAD(certList);
  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  PR_ASSERT(!(CERT_LIST_END(endEntityNode, certList) ||
              CERT_LIST_END(rootNode, certList)));
  if (CERT_LIST_END(endEntityNode, certList) ||
      CERT_LIST_END(rootNode, certList)) {
    return;
  }
  CERTCertificate* endEntityCert = endEntityNode->cert;
  PR_ASSERT(endEntityCert);
  if (!endEntityCert) {
    return;
  }

  // Only log telemetry if the root CA is built-in
  CERTCertificate* rootCert = rootNode->cert;
  PR_ASSERT(rootCert);
  if (!rootCert) {
    return;
  }
  bool isBuiltIn = false;
  Result rv = IsCertBuiltInRoot(rootCert, isBuiltIn);
  if (rv != Success || !isBuiltIn) {
    return;
  }

  // Find the EKU extension, if present
  bool foundEKU = false;
  SECOidTag oidTag;
  CERTCertExtension* ekuExtension = nullptr;
  for (size_t i = 0; endEntityCert->extensions && endEntityCert->extensions[i];
       i++) {
    oidTag = SECOID_FindOIDTag(&endEntityCert->extensions[i]->id);
    if (oidTag == SEC_OID_X509_EXT_KEY_USAGE) {
      foundEKU = true;
      ekuExtension = endEntityCert->extensions[i];
    }
  }

  if (!foundEKU) {
    Telemetry::Accumulate(Telemetry::SSL_SERVER_AUTH_EKU, 0);
    return;
  }

  // Parse the EKU extension
  UniqueCERTOidSequence ekuSequence(
    CERT_DecodeOidSequence(&ekuExtension->value));
  if (!ekuSequence) {
    return;
  }

  // Search through the available EKUs
  bool foundServerAuth = false;
  bool foundOther = false;
  for (SECItem** oids = ekuSequence->oids; oids && *oids; oids++) {
    oidTag = SECOID_FindOIDTag(*oids);
    if (oidTag == SEC_OID_EXT_KEY_USAGE_SERVER_AUTH) {
      foundServerAuth = true;
    } else {
      foundOther = true;
    }
  }

  // Cases 3 is included only for completeness.  It should never
  // appear in these statistics, because CheckExtendedKeyUsage()
  // should require the EKU extension, if present, to contain the
  // value id_kp_serverAuth.
  if (foundServerAuth && !foundOther) {
    Telemetry::Accumulate(Telemetry::SSL_SERVER_AUTH_EKU, 1);
  } else if (foundServerAuth && foundOther) {
    Telemetry::Accumulate(Telemetry::SSL_SERVER_AUTH_EKU, 2);
  } else if (!foundServerAuth) {
    Telemetry::Accumulate(Telemetry::SSL_SERVER_AUTH_EKU, 3);
  }
}

// Gathers telemetry on which CA is the root of a given cert chain.
// If the root is a built-in root, then the telemetry makes a count
// by root.  Roots that are not built-in are counted in one bin.
void
GatherRootCATelemetry(const UniqueCERTCertList& certList)
{
  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  PR_ASSERT(rootNode);
  if (!rootNode) {
    return;
  }
  PR_ASSERT(!CERT_LIST_END(rootNode, certList));
  if (CERT_LIST_END(rootNode, certList)) {
    return;
  }
  CERTCertificate* rootCert = rootNode->cert;
  PR_ASSERT(rootCert);
  if (!rootCert) {
    return;
  }
  AccumulateTelemetryForRootCA(Telemetry::CERT_VALIDATION_SUCCESS_BY_CA,
                               rootCert);
}

// These time are appoximate, i.e., doesn't account for leap seconds, etc
const uint64_t ONE_WEEK_IN_SECONDS = (7 * (24 * 60 *60));
const uint64_t ONE_YEAR_IN_WEEKS   = 52;

// Gathers telemetry on the certificate lifetimes we observe in the wild
void
GatherEndEntityTelemetry(const UniqueCERTCertList& certList)
{
  CERTCertListNode* endEntityNode = CERT_LIST_HEAD(certList);
  PR_ASSERT(endEntityNode);
  if (!endEntityNode) {
    return;
  }

  CERTCertificate * endEntityCert = endEntityNode->cert;
  PR_ASSERT(endEntityCert);
  if (!endEntityCert) {
    return;
  }

  PRTime notBefore;
  PRTime notAfter;

  if (CERT_GetCertTimes(endEntityCert, &notBefore, &notAfter) != SECSuccess) {
    return;
  }

  PR_ASSERT(notAfter > notBefore);
  if (notAfter <= notBefore) {
    return;
  }

  uint64_t durationInWeeks = (notAfter - notBefore)
    / PR_USEC_PER_SEC
    / ONE_WEEK_IN_SECONDS;

  if (durationInWeeks > (2 * ONE_YEAR_IN_WEEKS)) {
    durationInWeeks = (2 * ONE_YEAR_IN_WEEKS) + 1;
  }

  Telemetry::Accumulate(Telemetry::SSL_OBSERVED_END_ENTITY_CERTIFICATE_LIFETIME,
      durationInWeeks);
}

// There are various things that we want to measure about certificate
// chains that we accept.  This is a single entry point for all of them.
void
GatherSuccessfulValidationTelemetry(const UniqueCERTCertList& certList)
{
  GatherBaselineRequirementsTelemetry(certList);
  GatherEKUTelemetry(certList);
  GatherRootCATelemetry(certList);
  GatherEndEntityTelemetry(certList);
}

// Note: Takes ownership of |peerCertChain| if SECSuccess is not returned.
SECStatus
AuthCertificate(CertVerifier& certVerifier,
                nsNSSSocketInfo* infoObject,
                const UniqueCERTCertificate& cert,
                UniqueCERTCertList& peerCertChain,
                SECItem* stapledOCSPResponse,
                uint32_t providerFlags,
                Time time)
{
  MOZ_ASSERT(infoObject);
  MOZ_ASSERT(cert);

  SECStatus rv;

  // We want to avoid storing any intermediate cert information when browsing
  // in private, transient contexts.
  bool saveIntermediates =
    !(providerFlags & nsISocketProvider::NO_PERMANENT_STORAGE);

  SECOidTag evOidPolicy;
  UniqueCERTCertList certList;
  CertVerifier::OCSPStaplingStatus ocspStaplingStatus =
    CertVerifier::OCSP_STAPLING_NEVER_CHECKED;
  KeySizeStatus keySizeStatus = KeySizeStatus::NeverChecked;
  SHA1ModeResult sha1ModeResult = SHA1ModeResult::NeverChecked;
  PinningTelemetryInfo pinningTelemetryInfo;

  int flags = 0;
  if (!infoObject->SharedState().IsOCSPStaplingEnabled() ||
      !infoObject->SharedState().IsOCSPMustStapleEnabled()) {
    flags |= CertVerifier::FLAG_TLS_IGNORE_STATUS_REQUEST;
  }

  rv = certVerifier.VerifySSLServerCert(cert, stapledOCSPResponse,
                                        time, infoObject,
                                        infoObject->GetHostNameRaw(),
                                        certList, saveIntermediates, flags,
                                        &evOidPolicy, &ocspStaplingStatus,
                                        &keySizeStatus, &sha1ModeResult,
                                        &pinningTelemetryInfo);
  PRErrorCode savedErrorCode;
  if (rv != SECSuccess) {
    savedErrorCode = PR_GetError();
  }

  uint32_t evStatus = (rv != SECSuccess) ? 0                // 0 = Failure
                    : (evOidPolicy == SEC_OID_UNKNOWN) ? 1  // 1 = DV
                    : 2;                                    // 2 = EV
  Telemetry::Accumulate(Telemetry::CERT_EV_STATUS, evStatus);

  if (ocspStaplingStatus != CertVerifier::OCSP_STAPLING_NEVER_CHECKED) {
    Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, ocspStaplingStatus);
  }
  if (keySizeStatus != KeySizeStatus::NeverChecked) {
    Telemetry::Accumulate(Telemetry::CERT_CHAIN_KEY_SIZE_STATUS,
                          static_cast<uint32_t>(keySizeStatus));
  }
  if (sha1ModeResult != SHA1ModeResult::NeverChecked) {
    Telemetry::Accumulate(Telemetry::CERT_CHAIN_SHA1_POLICY_STATUS,
                          static_cast<uint32_t>(sha1ModeResult));
  }

  if (pinningTelemetryInfo.accumulateForRoot) {
    Telemetry::Accumulate(Telemetry::CERT_PINNING_FAILURES_BY_CA,
                          pinningTelemetryInfo.rootBucket);
  }

  if (pinningTelemetryInfo.accumulateResult) {
    Telemetry::Accumulate(pinningTelemetryInfo.certPinningResultHistogram,
                          pinningTelemetryInfo.certPinningResultBucket);
  }

  // We want to remember the CA certs in the temp db, so that the application can find the
  // complete chain at any time it might need it.
  // But we keep only those CA certs in the temp db, that we didn't already know.

  RefPtr<nsSSLStatus> status(infoObject->SSLStatus());
  RefPtr<nsNSSCertificate> nsc;

  if (!status || !status->HasServerCert()) {
    if( rv == SECSuccess ){
      nsc = nsNSSCertificate::Create(cert.get(), &evOidPolicy);
    }
    else {
      nsc = nsNSSCertificate::Create(cert.get());
    }
  }

  if (rv == SECSuccess) {
    GatherSuccessfulValidationTelemetry(certList);

    // The connection may get terminated, for example, if the server requires
    // a client cert. Let's provide a minimal SSLStatus
    // to the caller that contains at least the cert and its status.
    if (!status) {
      status = new nsSSLStatus();
      infoObject->SetSSLStatus(status);
    }

    if (rv == SECSuccess) {
      // Certificate verification succeeded delete any potential record
      // of certificate error bits.
      RememberCertErrorsTable::GetInstance().RememberCertHasError(infoObject,
                                                                  nullptr, rv);
    }
    else {
      // Certificate verification failed, update the status' bits.
      RememberCertErrorsTable::GetInstance().LookupCertErrorBits(
        infoObject, status);
    }

    if (status && !status->HasServerCert()) {
      nsNSSCertificate::EVStatus evStatus;
      if (evOidPolicy == SEC_OID_UNKNOWN || rv != SECSuccess) {
        evStatus = nsNSSCertificate::ev_status_invalid;
      } else {
        evStatus = nsNSSCertificate::ev_status_valid;
      }

      status->SetServerCert(nsc, evStatus);
      MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
             ("AuthCertificate setting NEW cert %p\n", nsc.get()));
    }
  }

  if (rv != SECSuccess) {
    // Certificate validation failed; store the peer certificate chain on
    // infoObject so it can be used for error reporting.
    infoObject->SetFailedCertChain(Move(peerCertChain));
    PR_SetError(savedErrorCode, 0);
  }

  return rv;
}

/*static*/ SECStatus
SSLServerCertVerificationJob::Dispatch(
  const RefPtr<SharedCertVerifier>& certVerifier,
  const void* fdForLogging,
  nsNSSSocketInfo* infoObject,
  const UniqueCERTCertificate& serverCert,
  const UniqueCERTCertList& peerCertChain,
  SECItem* stapledOCSPResponse,
  uint32_t providerFlags,
  Time time,
  PRTime prtime)
{
  // Runs on the socket transport thread
  if (!certVerifier || !infoObject || !serverCert) {
    NS_ERROR("Invalid parameters for SSL server cert validation");
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return SECFailure;
  }

  // Copy the certificate list so the runnable can take ownership of it in the
  // constructor.
  // We can safely skip checking if NSS has already shut down here since we're
  // in the middle of verifying a certificate.
  nsNSSShutDownPreventionLock lock;
  UniqueCERTCertList peerCertChainCopy =
    nsNSSCertList::DupCertList(peerCertChain, lock);
  if (!peerCertChainCopy) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return SECFailure;
  }

  RefPtr<SSLServerCertVerificationJob> job(
    new SSLServerCertVerificationJob(certVerifier, fdForLogging, infoObject,
                                     serverCert, Move(peerCertChainCopy),
                                     stapledOCSPResponse, providerFlags,
                                     time, prtime));

  nsresult nrv;
  if (!gCertVerificationThreadPool) {
    nrv = NS_ERROR_NOT_INITIALIZED;
  } else {
    nrv = gCertVerificationThreadPool->Dispatch(job, NS_DISPATCH_NORMAL);
  }
  if (NS_FAILED(nrv)) {
    // We can't call SetCertVerificationResult here to change
    // mCertVerificationState because SetCertVerificationResult will call
    // libssl functions that acquire SSL locks that are already being held at
    // this point. infoObject->mCertVerificationState will be stuck at
    // waiting_for_cert_verification here, but that is OK because we already
    // have to be able to handle cases where we encounter non-cert errors while
    // in that state.
    PRErrorCode error = nrv == NS_ERROR_OUT_OF_MEMORY
                      ? SEC_ERROR_NO_MEMORY
                      : PR_INVALID_STATE_ERROR;
    PORT_SetError(error);
    return SECFailure;
  }

  PORT_SetError(PR_WOULD_BLOCK_ERROR);
  return SECWouldBlock;
}

NS_IMETHODIMP
SSLServerCertVerificationJob::Run()
{
  // Runs on a cert verification thread

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] SSLServerCertVerificationJob::Run\n", mInfoObject.get()));

  PRErrorCode error;

  nsNSSShutDownPreventionLock nssShutdownPrevention;
  if (mInfoObject->isAlreadyShutDown()) {
    error = SEC_ERROR_USER_CANCELLED;
  } else {
    Telemetry::ID successTelemetry
      = Telemetry::SSL_SUCCESFUL_CERT_VALIDATION_TIME_MOZILLAPKIX;
    Telemetry::ID failureTelemetry
      = Telemetry::SSL_INITIAL_FAILED_CERT_VALIDATION_TIME_MOZILLAPKIX;

    // Reset the error code here so we can detect if AuthCertificate fails to
    // set the error code if/when it fails.
    PR_SetError(0, 0);
    SECStatus rv = AuthCertificate(*mCertVerifier, mInfoObject, mCert,
                                   mPeerCertChain, mStapledOCSPResponse.get(),
                                   mProviderFlags, mTime);
    MOZ_ASSERT(mPeerCertChain || rv != SECSuccess,
               "AuthCertificate() should take ownership of chain on failure");
    if (rv == SECSuccess) {
      uint32_t interval = (uint32_t) ((TimeStamp::Now() - mJobStartTime).ToMilliseconds());
      RefPtr<SSLServerCertVerificationResult> restart(
        new SSLServerCertVerificationResult(mInfoObject, 0,
                                            successTelemetry, interval));
      restart->Dispatch();
      Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, 1);
      return NS_OK;
    }

    // Note: the interval is not calculated once as PR_GetError MUST be called
    // before any other  function call
    error = PR_GetError();
    {
      TimeStamp now = TimeStamp::Now();
      MutexAutoLock telemetryMutex(*gSSLVerificationTelemetryMutex);
      Telemetry::AccumulateTimeDelta(failureTelemetry, mJobStartTime, now);
    }
    if (error != 0) {
      RefPtr<CertErrorRunnable> runnable(
          CreateCertErrorRunnable(*mCertVerifier, error, mInfoObject, mCert,
                                  mFdForLogging, mProviderFlags, mPRTime));
      if (!runnable) {
        // CreateCertErrorRunnable set a new error code
        error = PR_GetError();
      } else {
        // We must block the the socket transport service thread while the
        // main thread executes the CertErrorRunnable. The CertErrorRunnable
        // will dispatch the result asynchronously, so we don't have to block
        // this thread waiting for it.

        MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
                ("[%p][%p] Before dispatching CertErrorRunnable\n",
                mFdForLogging, runnable.get()));

        nsresult nrv;
        nsCOMPtr<nsIEventTarget> stsTarget
          = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &nrv);
        if (NS_SUCCEEDED(nrv)) {
          nrv = stsTarget->Dispatch(new CertErrorRunnableRunnable(runnable),
                                    NS_DISPATCH_NORMAL);
        }
        if (NS_SUCCEEDED(nrv)) {
          return NS_OK;
        }

        NS_ERROR("Failed to dispatch CertErrorRunnable");
        error = PR_INVALID_STATE_ERROR;
      }
    }
  }

  if (error == 0) {
    NS_NOTREACHED("no error set during certificate validation failure");
    error = PR_INVALID_STATE_ERROR;
  }

  RefPtr<SSLServerCertVerificationResult> failure(
    new SSLServerCertVerificationResult(mInfoObject, error));
  failure->Dispatch();
  return NS_OK;
}

} // unnamed namespace

// Extracts whatever information we need out of fd (using SSL_*) and passes it
// to SSLServerCertVerificationJob::Dispatch. SSLServerCertVerificationJob should
// never do anything with fd except logging.
SECStatus
AuthCertificateHook(void* arg, PRFileDesc* fd, PRBool checkSig, PRBool isServer)
{
  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  if (!certVerifier) {
    PR_SetError(SEC_ERROR_NOT_INITIALIZED, 0);
    return SECFailure;
  }

  // Runs on the socket transport thread

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
         ("[%p] starting AuthCertificateHook\n", fd));

  // Modern libssl always passes PR_TRUE for checkSig, and we have no means of
  // doing verification without checking signatures.
  NS_ASSERTION(checkSig, "AuthCertificateHook: checkSig unexpectedly false");

  // PSM never causes libssl to call this function with PR_TRUE for isServer,
  // and many things in PSM assume that we are a client.
  NS_ASSERTION(!isServer, "AuthCertificateHook: isServer unexpectedly true");

  nsNSSSocketInfo* socketInfo = static_cast<nsNSSSocketInfo*>(arg);

  UniqueCERTCertificate serverCert(SSL_PeerCertificate(fd));

  if (!checkSig || isServer || !socketInfo || !serverCert) {
      PR_SetError(PR_INVALID_STATE_ERROR, 0);
      return SECFailure;
  }

  // Get the peer certificate chain for error reporting
  UniqueCERTCertList peerCertChain(SSL_PeerCertificateChain(fd));
  if (!peerCertChain) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  socketInfo->SetFullHandshake();

  Time now(Now());
  PRTime prnow(PR_Now());

  if (BlockServerCertChangeForSpdy(socketInfo, serverCert) != SECSuccess)
    return SECFailure;

  nsCOMPtr<nsISSLSocketControl> sslSocketControl = do_QueryInterface(
    NS_ISUPPORTS_CAST(nsITransportSecurityInfo*, socketInfo));
  if (sslSocketControl && sslSocketControl->GetBypassAuthentication()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("[%p] Bypass Auth in AuthCertificateHook\n", fd));
    return SECSuccess;
  }

  bool onSTSThread;
  nsresult nrv;
  nsCOMPtr<nsIEventTarget> sts
    = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &nrv);
  if (NS_SUCCEEDED(nrv)) {
    nrv = sts->IsOnCurrentThread(&onSTSThread);
  }

  if (NS_FAILED(nrv)) {
    NS_ERROR("Could not get STS service or IsOnCurrentThread failed");
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return SECFailure;
  }

  // SSL_PeerStapledOCSPResponses will never return a non-empty response if
  // OCSP stapling wasn't enabled because libssl wouldn't have let the server
  // return a stapled OCSP response.
  // We don't own these pointers.
  const SECItemArray* csa = SSL_PeerStapledOCSPResponses(fd);
  SECItem* stapledOCSPResponse = nullptr;
  // we currently only support single stapled responses
  if (csa && csa->len == 1) {
    stapledOCSPResponse = &csa->items[0];
  }

  uint32_t providerFlags = 0;
  socketInfo->GetProviderFlags(&providerFlags);

  if (onSTSThread) {

    // We *must* do certificate verification on a background thread because
    // we need the socket transport thread to be free for our OCSP requests,
    // and we *want* to do certificate verification on a background thread
    // because of the performance benefits of doing so.
    socketInfo->SetCertVerificationWaiting();
    SECStatus rv = SSLServerCertVerificationJob::Dispatch(
                     certVerifier, static_cast<const void*>(fd), socketInfo,
                     serverCert, peerCertChain, stapledOCSPResponse,
                     providerFlags, now, prnow);
    return rv;
  }

  // We can't do certificate verification on a background thread, because the
  // thread doing the network I/O may not interrupt its network I/O on receipt
  // of our SSLServerCertVerificationResult event, and/or it might not even be
  // a non-blocking socket.

  SECStatus rv = AuthCertificate(*certVerifier, socketInfo, serverCert,
                                 peerCertChain, stapledOCSPResponse,
                                 providerFlags, now);
  MOZ_ASSERT(peerCertChain || rv != SECSuccess,
             "AuthCertificate() should take ownership of chain on failure");
  if (rv == SECSuccess) {
    Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, 1);
    return SECSuccess;
  }

  PRErrorCode error = PR_GetError();
  if (error != 0) {
    RefPtr<CertErrorRunnable> runnable(
        CreateCertErrorRunnable(*certVerifier, error, socketInfo, serverCert,
                                static_cast<const void*>(fd), providerFlags,
                                prnow));
    if (!runnable) {
      // CreateCertErrorRunnable sets a new error code when it fails
      error = PR_GetError();
    } else {
      // We have to return SECSuccess or SECFailure based on the result of the
      // override processing, so we must block this thread waiting for it. The
      // CertErrorRunnable will NOT dispatch the result at all, since we passed
      // false for CreateCertErrorRunnable's async parameter
      nrv = runnable->DispatchToMainThreadAndWait();
      if (NS_FAILED(nrv)) {
        NS_ERROR("Failed to dispatch CertErrorRunnable");
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return SECFailure;
      }

      if (!runnable->mResult) {
        NS_ERROR("CertErrorRunnable did not set result");
        PR_SetError(PR_INVALID_STATE_ERROR, 0);
        return SECFailure;
      }

      if (runnable->mResult->mErrorCode == 0) {
        return SECSuccess; // cert error override occurred.
      }

      // We must call SetCanceled here to set the error message type
      // in case it isn't PlainErrorMessage, which is what we would
      // default to if we just called
      // PR_SetError(runnable->mResult->mErrorCode, 0) and returned
      // SECFailure without doing this.
      socketInfo->SetCanceled(runnable->mResult->mErrorCode,
                              runnable->mResult->mErrorMessageType);
      error = runnable->mResult->mErrorCode;
    }
  }

  if (error == 0) {
    NS_ERROR("error code not set");
    error = PR_UNKNOWN_ERROR;
  }

  PR_SetError(error, 0);
  return SECFailure;
}

#ifndef MOZ_NO_EV_CERTS
class InitializeIdentityInfo : public CryptoTask
{
  virtual nsresult CalculateResult() override
  {
    EnsureIdentityInfoLoaded();
    return NS_OK;
  }

  virtual void ReleaseNSSResources() override { } // no-op
  virtual void CallCallback(nsresult rv) override { } // no-op
};
#endif

void EnsureServerVerificationInitialized()
{
#ifndef MOZ_NO_EV_CERTS
  // Should only be called from socket transport thread due to the static
  // variable and the reference to gCertVerificationThreadPool

  static bool triggeredCertVerifierInit = false;
  if (triggeredCertVerifierInit)
    return;
  triggeredCertVerifierInit = true;

  RefPtr<InitializeIdentityInfo> initJob = new InitializeIdentityInfo();
  if (gCertVerificationThreadPool)
    gCertVerificationThreadPool->Dispatch(initJob, NS_DISPATCH_NORMAL);
#endif
}

SSLServerCertVerificationResult::SSLServerCertVerificationResult(
        nsNSSSocketInfo* infoObject, PRErrorCode errorCode,
        Telemetry::ID telemetryID, uint32_t telemetryValue,
        SSLErrorMessageType errorMessageType)
  : mInfoObject(infoObject)
  , mErrorCode(errorCode)
  , mErrorMessageType(errorMessageType)
  , mTelemetryID(telemetryID)
  , mTelemetryValue(telemetryValue)
{
// We accumulate telemetry for (only) successful validations on the main thread
// to avoid adversely affecting performance by acquiring the mutex that we use
// when accumulating the telemetry for unsuccessful validations. Unsuccessful
// validations times are accumulated elsewhere.
MOZ_ASSERT(telemetryID == Telemetry::HistogramCount || errorCode == 0);
}

void
SSLServerCertVerificationResult::Dispatch()
{
  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsTarget
    = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  NS_ASSERTION(stsTarget,
               "Failed to get socket transport service event target");
  rv = stsTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "Failed to dispatch SSLServerCertVerificationResult");
}

NS_IMETHODIMP
SSLServerCertVerificationResult::Run()
{
  // TODO: Assert that we're on the socket transport thread
  if (mTelemetryID != Telemetry::HistogramCount) {
     Telemetry::Accumulate(mTelemetryID, mTelemetryValue);
  }
  // XXX: This cast will be removed by the next patch
  ((nsNSSSocketInfo*) mInfoObject.get())
    ->SetCertVerificationResult(mErrorCode, mErrorMessageType);
  return NS_OK;
}

} } // namespace mozilla::psm

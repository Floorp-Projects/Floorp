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
// and then immediatley return SECWouldBlock to libssl. These jobs are where
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

#include "pkix/pkixtypes.h"
#include "CertVerifier.h"
#include "CryptoTask.h"
#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "nsIBadCertListener2.h"
#include "nsICertOverrideService.h"
#include "nsISiteSecurityService.h"
#include "nsNSSComponent.h"
#include "nsNSSCleaner.h"
#include "nsRecentBadCerts.h"
#include "nsNSSIOLayer.h"
#include "nsNSSShutDown.h"

#include "mozilla/Assertions.h"
#include "mozilla/Mutex.h"
#include "mozilla/Telemetry.h"
#include "mozilla/unused.h"
#include "nsIThreadPool.h"
#include "nsNetUtil.h"
#include "nsXPCOMCIDInternal.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "PSMRunnable.h"
#include "SharedSSLState.h"
#include "nsContentUtils.h"

#include "ssl.h"
#include "secerr.h"
#include "secport.h"
#include "sslerr.h"
#include "ocsp.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

namespace mozilla { namespace psm {

namespace {

NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)
NSSCleanupAutoPtrClass_WithParam(PLArenaPool, PORT_FreeArena, FalseParam, false)

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
LogInvalidCertError(TransportSecurityInfo* socketInfo,
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
class SSLServerCertVerificationResult : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  SSLServerCertVerificationResult(TransportSecurityInfo* infoObject,
                                  PRErrorCode errorCode,
                                  Telemetry::ID telemetryID = Telemetry::HistogramCount,
                                  uint32_t telemetryValue = -1,
                                  SSLErrorMessageType errorMessageType =
                                      PlainErrorMessage);

  void Dispatch();
private:
  const RefPtr<TransportSecurityInfo> mInfoObject;
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
                    TransportSecurityInfo* infoObject,
                    PRErrorCode defaultErrorCodeToReport,
                    uint32_t collectedErrors,
                    PRErrorCode errorCodeTrust,
                    PRErrorCode errorCodeMismatch,
                    PRErrorCode errorCodeExpired,
                    uint32_t providerFlags)
    : mFdForLogging(fdForLogging), mCert(cert), mInfoObject(infoObject),
      mDefaultErrorCodeToReport(defaultErrorCodeToReport),
      mCollectedErrors(collectedErrors),
      mErrorCodeTrust(errorCodeTrust),
      mErrorCodeMismatch(errorCodeMismatch),
      mErrorCodeExpired(errorCodeExpired),
      mProviderFlags(providerFlags)
  {
  }

  virtual void RunOnTargetThread();
  RefPtr<SSLServerCertVerificationResult> mResult; // out
private:
  SSLServerCertVerificationResult* CheckCertOverrides();

  const void* const mFdForLogging; // may become an invalid pointer; do not dereference
  const nsCOMPtr<nsIX509Cert> mCert;
  const RefPtr<TransportSecurityInfo> mInfoObject;
  const PRErrorCode mDefaultErrorCodeToReport;
  const uint32_t mCollectedErrors;
  const PRErrorCode mErrorCodeTrust;
  const PRErrorCode mErrorCodeMismatch;
  const PRErrorCode mErrorCodeExpired;
  const uint32_t mProviderFlags;
};

// A probe value of 1 means "no error".
uint32_t
MapCertErrorToProbeValue(PRErrorCode errorCode)
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
  }
  NS_WARNING("Unknown certificate error code. Does MapCertErrorToProbeValue "
             "handle everything in PRErrorCodeToOverrideType?");
  return 0;
}

SECStatus
MozillaPKIXDetermineCertOverrideErrors(CERTCertificate* cert,
                                       const char* hostName, PRTime now,
                                       PRErrorCode defaultErrorCodeToReport,
                                       /*out*/ uint32_t& collectedErrors,
                                       /*out*/ PRErrorCode& errorCodeTrust,
                                       /*out*/ PRErrorCode& errorCodeMismatch,
                                       /*out*/ PRErrorCode& errorCodeExpired)
{
  MOZ_ASSERT(cert);
  MOZ_ASSERT(hostName);
  MOZ_ASSERT(collectedErrors == 0);
  MOZ_ASSERT(errorCodeTrust == 0);
  MOZ_ASSERT(errorCodeMismatch == 0);
  MOZ_ASSERT(errorCodeExpired == 0);

  // Assumes the error prioritization described in mozilla::pkix's
  // BuildForward function. Also assumes that CERT_VerifyCertName was only
  // called if CertVerifier::VerifyCert succeeded.
  switch (defaultErrorCodeToReport) {
    case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
    case SEC_ERROR_UNKNOWN_ISSUER:
    {
      collectedErrors = nsICertOverrideService::ERROR_UNTRUSTED;
      errorCodeTrust = defaultErrorCodeToReport;

      SECCertTimeValidity validity = CERT_CheckCertValidTimes(cert, now, false);
      if (validity == secCertTimeUndetermined) {
        PR_SetError(defaultErrorCodeToReport, 0);
        return SECFailure;
      }
      if (validity != secCertTimeValid) {
        collectedErrors |= nsICertOverrideService::ERROR_TIME;
        errorCodeExpired = SEC_ERROR_EXPIRED_CERTIFICATE;
      }
      break;
    }

    case SEC_ERROR_EXPIRED_CERTIFICATE:
      collectedErrors = nsICertOverrideService::ERROR_TIME;
      errorCodeExpired = SEC_ERROR_EXPIRED_CERTIFICATE;
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
    if (CERT_VerifyCertName(cert, hostName) != SECSuccess) {
      if (PR_GetError() != SSL_ERROR_BAD_CERT_DOMAIN) {
        PR_SetError(defaultErrorCodeToReport, 0);
        return SECFailure;
      }

      collectedErrors |= nsICertOverrideService::ERROR_MISMATCH;
      errorCodeMismatch = SSL_ERROR_BAD_CERT_DOMAIN;
    }
  }

  return SECSuccess;
}

SSLServerCertVerificationResult*
CertErrorRunnable::CheckCertOverrides()
{
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p][%p] top of CheckCertOverrides\n",
                                    mFdForLogging, this));
  // "Use" mFdForLogging in non-PR_LOGGING builds, too, to suppress
  // clang's -Wunused-private-field build warning for this variable:
  unused << mFdForLogging;

  if (!NS_IsMainThread()) {
    NS_ERROR("CertErrorRunnable::CheckCertOverrides called off main thread");
    return new SSLServerCertVerificationResult(mInfoObject,
                                               mDefaultErrorCodeToReport);
  }

  int32_t port;
  mInfoObject->GetPort(&port);

  nsCString hostWithPortString;
  hostWithPortString.AppendASCII(mInfoObject->GetHostNameRaw());
  hostWithPortString.AppendLiteral(":");
  hostWithPortString.AppendInt(port);

  uint32_t remaining_display_errors = mCollectedErrors;

  nsresult nsrv;

  // Enforce Strict-Transport-Security for hosts that are "STS" hosts:
  // connections must be dropped when there are any certificate errors
  // (STS Spec section 7.3).
  bool strictTransportSecurityEnabled = false;
  nsCOMPtr<nsISiteSecurityService> sss
    = do_GetService(NS_SSSERVICE_CONTRACTID, &nsrv);
  if (NS_SUCCEEDED(nsrv)) {
    nsrv = sss->IsSecureHost(nsISiteSecurityService::HEADER_HSTS,
                             mInfoObject->GetHostNameRaw(),
                             mProviderFlags,
                             &strictTransportSecurityEnabled);
  }
  if (NS_FAILED(nsrv)) {
    return new SSLServerCertVerificationResult(mInfoObject,
                                               mDefaultErrorCodeToReport);
  }

  if (!strictTransportSecurityEnabled) {
    nsCOMPtr<nsICertOverrideService> overrideService =
      do_GetService(NS_CERTOVERRIDE_CONTRACTID);
    // it is fine to continue without the nsICertOverrideService

    uint32_t overrideBits = 0;

    if (overrideService)
    {
      bool haveOverride;
      bool isTemporaryOverride; // we don't care
      nsCString hostString(mInfoObject->GetHostName());
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
        uint32_t probeValue = MapCertErrorToProbeValue(mErrorCodeTrust);
        Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, probeValue);
      }
      if (mErrorCodeMismatch != 0) {
        uint32_t probeValue = MapCertErrorToProbeValue(mErrorCodeMismatch);
        Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, probeValue);
      }
      if (mErrorCodeExpired != 0) {
        uint32_t probeValue = MapCertErrorToProbeValue(mErrorCodeExpired);
        Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, probeValue);
      }

      // all errors are covered by override rules, so let's accept the cert
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("[%p][%p] All errors covered by override rules\n",
             mFdForLogging, this));
      return new SSLServerCertVerificationResult(mInfoObject, 0);
    }
  } else {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("[%p][%p] Strict-Transport-Security is violated: untrusted "
            "transport layer\n", mFdForLogging, this));
  }

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
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

  nsCOMPtr<nsIX509CertDB> certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
  nsCOMPtr<nsIRecentBadCerts> recentBadCertsService;
  if (certdb) {
    bool isPrivate = mProviderFlags & nsISocketProvider::NO_PERMANENT_STORAGE;
    certdb->GetRecentBadCerts(isPrivate, getter_AddRefs(recentBadCertsService));
  }

  if (recentBadCertsService) {
    NS_ConvertUTF8toUTF16 hostWithPortStringUTF16(hostWithPortString);
    recentBadCertsService->AddBadCert(hostWithPortStringUTF16,
                                      mInfoObject->SSLStatus());
  }

  // pick the error code to report by priority
  PRErrorCode errorCodeToReport = mErrorCodeTrust    ? mErrorCodeTrust
                                : mErrorCodeMismatch ? mErrorCodeMismatch
                                : mErrorCodeExpired  ? mErrorCodeExpired
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

// Converts a PRErrorCode into one of
//   nsICertOverrideService::ERROR_UNTRUSTED,
//   nsICertOverrideService::ERROR_MISMATCH,
//   nsICertOverrideService::ERROR_TIME
// if the given error code is an overridable error.
// If it is not, then 0 is returned.
uint32_t
PRErrorCodeToOverrideType(PRErrorCode errorCode)
{
  switch (errorCode)
  {
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
    case SEC_ERROR_UNTRUSTED_ISSUER:
    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
    case SEC_ERROR_UNTRUSTED_CERT:
    case SEC_ERROR_INADEQUATE_KEY_USAGE:
    case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
      // We group all these errors as "cert not trusted"
      return nsICertOverrideService::ERROR_UNTRUSTED;
    case SSL_ERROR_BAD_CERT_DOMAIN:
      return nsICertOverrideService::ERROR_MISMATCH;
    case SEC_ERROR_EXPIRED_CERTIFICATE:
      return nsICertOverrideService::ERROR_TIME;
    default:
      return 0;
  }
}

SECStatus
NSSDetermineCertOverrideErrors(CertVerifier& certVerifier,
                               CERTCertificate* cert,
                               const SECItem* stapledOCSPResponse,
                               TransportSecurityInfo* infoObject,
                               PRTime now,
                               PRErrorCode defaultErrorCodeToReport,
                               /*out*/ uint32_t& collectedErrors,
                               /*out*/ PRErrorCode& errorCodeTrust,
                               /*out*/ PRErrorCode& errorCodeMismatch,
                               /*out*/ PRErrorCode& errorCodeExpired)
{
  MOZ_ASSERT(cert);
  MOZ_ASSERT(infoObject);
  MOZ_ASSERT(defaultErrorCodeToReport != 0);
  MOZ_ASSERT(collectedErrors == 0);
  MOZ_ASSERT(errorCodeTrust == 0);
  MOZ_ASSERT(errorCodeMismatch == 0);
  MOZ_ASSERT(errorCodeExpired == 0);

  if (defaultErrorCodeToReport == 0) {
    NS_ERROR("No error code set during certificate validation failure.");
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  // We only allow overrides for certain errors. Return early if the error
  // is not one of them. This is to avoid doing revocation fetching in the
  // case of OCSP stapling and probably for other reasons.
  if (PRErrorCodeToOverrideType(defaultErrorCodeToReport) == 0) {
    PR_SetError(defaultErrorCodeToReport, 0);
    return SECFailure;
  }

  PLArenaPool* log_arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  PLArenaPoolCleanerFalseParam log_arena_cleaner(log_arena);
  if (!log_arena) {
    NS_ERROR("PORT_NewArena failed");
    return SECFailure; // PORT_NewArena set error code
  }

  CERTVerifyLog* verify_log = PORT_ArenaZNew(log_arena, CERTVerifyLog);
  if (!verify_log) {
    NS_ERROR("PORT_ArenaZNew failed");
    return SECFailure; // PORT_ArenaZNew set error code
  }
  CERTVerifyLogContentsCleaner verify_log_cleaner(verify_log);
  verify_log->arena = log_arena;

  // We ignore the result code of the cert verification (i.e. VerifyCert's rv)
  // Either it is a failure, which is expected, and we'll process the
  //                         verify log below.
  // Or it is a success, then a domain mismatch is the only
  //                     possible failure.
  // XXX TODO: convert to VerifySSLServerCert
  // XXX TODO: get rid of error log
  certVerifier.VerifyCert(cert, stapledOCSPResponse, certificateUsageSSLServer,
                          now, infoObject, 0, nullptr, nullptr, verify_log);

  // Check the name field against the desired hostname.
  if (CERT_VerifyCertName(cert, infoObject->GetHostNameRaw()) != SECSuccess) {
    collectedErrors |= nsICertOverrideService::ERROR_MISMATCH;
    errorCodeMismatch = SSL_ERROR_BAD_CERT_DOMAIN;
  }

  CERTVerifyLogNode* i_node;
  for (i_node = verify_log->head; i_node; i_node = i_node->next) {
    uint32_t overrideType = PRErrorCodeToOverrideType(i_node->error);
    // If this isn't an overridable error, set the error and return.
    if (overrideType == 0) {
      PR_SetError(i_node->error, 0);
      return SECFailure;
    }
    collectedErrors |= overrideType;
    if (overrideType == nsICertOverrideService::ERROR_UNTRUSTED) {
      if (errorCodeTrust == 0) {
        errorCodeTrust = i_node->error;
      }
    } else if (overrideType == nsICertOverrideService::ERROR_MISMATCH) {
      if (errorCodeMismatch == 0) {
        errorCodeMismatch = i_node->error;
      }
    } else if (overrideType == nsICertOverrideService::ERROR_TIME) {
      if (errorCodeExpired == 0) {
        errorCodeExpired = i_node->error;
      }
    } else {
      MOZ_CRASH("unexpected return value from PRErrorCodeToOverrideType");
    }
  }

  return SECSuccess;
}

// Returns null with the error code (PR_GetError()) set if it does not create
// the CertErrorRunnable.
CertErrorRunnable*
CreateCertErrorRunnable(CertVerifier& certVerifier,
                        PRErrorCode defaultErrorCodeToReport,
                        TransportSecurityInfo* infoObject,
                        CERTCertificate* cert,
                        const SECItem* stapledOCSPResponse,
                        const void* fdForLogging,
                        uint32_t providerFlags,
                        PRTime now)
{
  MOZ_ASSERT(infoObject);
  MOZ_ASSERT(cert);

  uint32_t collected_errors = 0;
  PRErrorCode errorCodeTrust = 0;
  PRErrorCode errorCodeMismatch = 0;
  PRErrorCode errorCodeExpired = 0;

  SECStatus rv;
  switch (certVerifier.mImplementation) {
    case CertVerifier::classic:
#ifndef NSS_NO_LIBPKIX
    case CertVerifier::libpkix:
#endif
      rv = NSSDetermineCertOverrideErrors(certVerifier, cert, stapledOCSPResponse,
                                          infoObject, now,
                                          defaultErrorCodeToReport,
                                          collected_errors, errorCodeTrust,
                                          errorCodeMismatch, errorCodeExpired);
      break;

    case CertVerifier::mozillapkix:
      rv = MozillaPKIXDetermineCertOverrideErrors(cert,
                                                  infoObject->GetHostNameRaw(),
                                                  now, defaultErrorCodeToReport,
                                                  collected_errors,
                                                  errorCodeTrust,
                                                  errorCodeMismatch,
                                                  errorCodeExpired);
      break;

    default:
      MOZ_CRASH("unexpected CertVerifier implementation");
      PR_SetError(defaultErrorCodeToReport, 0);
      return nullptr;

  }
  if (rv != SECSuccess) {
    return nullptr;
  }

  RefPtr<nsNSSCertificate> nssCert(nsNSSCertificate::Create(cert));
  if (!nssCert) {
    NS_ERROR("nsNSSCertificate::Create failed");
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return nullptr;
  }

  if (!collected_errors) {
    // This will happen when CERT_*Verify* only returned error(s) that are
    // not on our whitelist of overridable certificate errors.
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] !collected_errors: %d\n",
           fdForLogging, static_cast<int>(defaultErrorCodeToReport)));
    PR_SetError(defaultErrorCodeToReport, 0);
    return nullptr;
  }

  infoObject->SetStatusErrorBits(*nssCert, collected_errors);

  return new CertErrorRunnable(fdForLogging,
                               static_cast<nsIX509Cert*>(nssCert.get()),
                               infoObject, defaultErrorCodeToReport,
                               collected_errors, errorCodeTrust,
                               errorCodeMismatch, errorCodeExpired,
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
class CertErrorRunnableRunnable : public nsRunnable
{
public:
  CertErrorRunnableRunnable(CertErrorRunnable* certErrorRunnable)
    : mCertErrorRunnable(certErrorRunnable)
  {
  }
private:
  NS_IMETHOD Run()
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

class SSLServerCertVerificationJob : public nsRunnable
{
public:
  // Must be called only on the socket transport thread
  static SECStatus Dispatch(const RefPtr<SharedCertVerifier>& certVerifier,
                            const void* fdForLogging,
                            TransportSecurityInfo* infoObject,
                            CERTCertificate* serverCert,
                            SECItem* stapledOCSPResponse,
                            uint32_t providerFlags,
                            PRTime time);
private:
  NS_DECL_NSIRUNNABLE

  // Must be called only on the socket transport thread
  SSLServerCertVerificationJob(const RefPtr<SharedCertVerifier>& certVerifier,
                               const void* fdForLogging,
                               TransportSecurityInfo* infoObject,
                               CERTCertificate* cert,
                               SECItem* stapledOCSPResponse,
                               uint32_t providerFlags,
                               PRTime time);
  const RefPtr<SharedCertVerifier> mCertVerifier;
  const void* const mFdForLogging;
  const RefPtr<TransportSecurityInfo> mInfoObject;
  const mozilla::pkix::ScopedCERTCertificate mCert;
  const uint32_t mProviderFlags;
  const PRTime mTime;
  const TimeStamp mJobStartTime;
  const ScopedSECItem mStapledOCSPResponse;
};

SSLServerCertVerificationJob::SSLServerCertVerificationJob(
    const RefPtr<SharedCertVerifier>& certVerifier, const void* fdForLogging,
    TransportSecurityInfo* infoObject, CERTCertificate* cert,
    SECItem* stapledOCSPResponse, uint32_t providerFlags, PRTime time)
  : mCertVerifier(certVerifier)
  , mFdForLogging(fdForLogging)
  , mInfoObject(infoObject)
  , mCert(CERT_DupCertificate(cert))
  , mProviderFlags(providerFlags)
  , mTime(time)
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
                             CERTCertificate* serverCert)
{
  // Get the existing cert. If there isn't one, then there is
  // no cert change to worry about.
  nsCOMPtr<nsIX509Cert> cert;
  nsCOMPtr<nsIX509Cert2> cert2;

  RefPtr<nsSSLStatus> status(infoObject->SSLStatus());
  if (!status) {
    // If we didn't have a status, then this is the
    // first handshake on this connection, not a
    // renegotiation.
    return SECSuccess;
  }

  status->GetServerCert(getter_AddRefs(cert));
  cert2 = do_QueryInterface(cert);
  if (!cert2) {
    NS_NOTREACHED("every nsSSLStatus must have a cert"
                  "that implements nsIX509Cert2");
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  // Filter out sockets that did not neogtiate SPDY via NPN
  nsAutoCString negotiatedNPN;
  nsresult rv = infoObject->GetNegotiatedNPN(negotiatedNPN);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "GetNegotiatedNPN() failed during renegotiation");

  if (NS_SUCCEEDED(rv) && !StringBeginsWith(negotiatedNPN,
                                            NS_LITERAL_CSTRING("spdy/")))
    return SECSuccess;

  // If GetNegotiatedNPN() failed we will assume spdy for safety's safe
  if (NS_FAILED(rv)) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("BlockServerCertChangeForSpdy failed GetNegotiatedNPN() call."
            " Assuming spdy.\n"));
  }

  // Check to see if the cert has actually changed
  ScopedCERTCertificate c(cert2->GetCert());
  NS_ASSERTION(c, "very bad and hopefully impossible state");
  bool sameCert = CERT_CompareCerts(c, serverCert);
  if (sameCert)
    return SECSuccess;

  // Report an error - changed cert is confirmed
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
         ("SPDY Refused to allow new cert during renegotiation\n"));
  PR_SetError(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED, 0);
  return SECFailure;
}

SECStatus
AuthCertificate(CertVerifier& certVerifier, TransportSecurityInfo* infoObject,
                CERTCertificate* cert, SECItem* stapledOCSPResponse,
                uint32_t providerFlags, PRTime time)
{
  MOZ_ASSERT(infoObject);
  MOZ_ASSERT(cert);

  SECStatus rv;

  // TODO: Remove this after we switch to mozilla::pkix as the
  // only option
  if (certVerifier.mImplementation == CertVerifier::classic) {
    if (stapledOCSPResponse) {
      CERTCertDBHandle* handle = CERT_GetDefaultCertDB();
      rv = CERT_CacheOCSPResponseFromSideChannel(handle, cert, PR_Now(),
                                                 stapledOCSPResponse,
                                                 infoObject);
      if (rv != SECSuccess) {
        // Due to buggy servers that will staple expired OCSP responses
        // (see for example http://trac.nginx.org/nginx/ticket/425),
        // don't terminate the connection if the stapled response is expired.
        // We will fall back to fetching revocation information.
        PRErrorCode ocspErrorCode = PR_GetError();
        if (ocspErrorCode != SEC_ERROR_OCSP_OLD_RESPONSE) {
          // stapled OCSP response present but invalid for some reason
          Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, 4);
          return rv;
        } else {
          // stapled OCSP response present but expired
          Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, 3);
        }
      } else {
        // stapled OCSP response present and good
        Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, 1);
      }
    } else {
      // no stapled OCSP response
      Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, 2);

      uint32_t reasonsForNotFetching = 0;

      char* ocspURI = CERT_GetOCSPAuthorityInfoAccessLocation(cert);
      if (!ocspURI) {
        reasonsForNotFetching |= 1; // invalid/missing OCSP URI
      } else {
        if (std::strncmp(ocspURI, "http://", 7)) { // approximation
          reasonsForNotFetching |= 1; // invalid/missing OCSP URI
        }
        PORT_Free(ocspURI);
      }

      if (!certVerifier.mOCSPDownloadEnabled) {
        reasonsForNotFetching |= 2;
      }

      Telemetry::Accumulate(Telemetry::SSL_OCSP_MAY_FETCH,
                            reasonsForNotFetching);
    }
  }

  // We want to avoid storing any intermediate cert information when browsing
  // in private, transient contexts.
  bool saveIntermediates =
    !(providerFlags & nsISocketProvider::NO_PERMANENT_STORAGE);

  mozilla::pkix::ScopedCERTCertList certList;
  SECOidTag evOidPolicy;
  rv = certVerifier.VerifySSLServerCert(cert, stapledOCSPResponse,
                                        time, infoObject,
                                        infoObject->GetHostNameRaw(),
                                        saveIntermediates, nullptr,
                                        &evOidPolicy);

  // We want to remember the CA certs in the temp db, so that the application can find the
  // complete chain at any time it might need it.
  // But we keep only those CA certs in the temp db, that we didn't already know.

  RefPtr<nsSSLStatus> status(infoObject->SSLStatus());
  RefPtr<nsNSSCertificate> nsc;

  if (!status || !status->mServerCert) {
    if( rv == SECSuccess ){
      nsc = nsNSSCertificate::Create(cert, &evOidPolicy);
    }
    else {
      nsc = nsNSSCertificate::Create(cert);
    }
  }

  if (rv == SECSuccess) {
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

    if (status && !status->mServerCert) {
      status->mServerCert = nsc;
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("AuthCertificate setting NEW cert %p\n", status->mServerCert.get()));
    }
  }

  return rv;
}

/*static*/ SECStatus
SSLServerCertVerificationJob::Dispatch(
  const RefPtr<SharedCertVerifier>& certVerifier,
  const void* fdForLogging,
  TransportSecurityInfo* infoObject,
  CERTCertificate* serverCert,
  SECItem* stapledOCSPResponse,
  uint32_t providerFlags,
  PRTime time)
{
  // Runs on the socket transport thread
  if (!certVerifier || !infoObject || !serverCert) {
    NS_ERROR("Invalid parameters for SSL server cert validation");
    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    return SECFailure;
  }

  RefPtr<SSLServerCertVerificationJob> job(
    new SSLServerCertVerificationJob(certVerifier, fdForLogging, infoObject,
                                     serverCert, stapledOCSPResponse,
                                     providerFlags, time));

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

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
          ("[%p] SSLServerCertVerificationJob::Run\n", mInfoObject.get()));

  PRErrorCode error;

  nsNSSShutDownPreventionLock nssShutdownPrevention;
  if (mInfoObject->isAlreadyShutDown()) {
    error = SEC_ERROR_USER_CANCELLED;
  } else {
    Telemetry::ID successTelemetry;
    Telemetry::ID failureTelemetry;
    switch (mCertVerifier->mImplementation) {
      case CertVerifier::classic:
        successTelemetry
          = Telemetry::SSL_SUCCESFUL_CERT_VALIDATION_TIME_CLASSIC;
        failureTelemetry
          = Telemetry::SSL_INITIAL_FAILED_CERT_VALIDATION_TIME_CLASSIC;
        break;
      case CertVerifier::mozillapkix:
        successTelemetry
          = Telemetry::SSL_SUCCESFUL_CERT_VALIDATION_TIME_MOZILLAPKIX;
        failureTelemetry
          = Telemetry::SSL_INITIAL_FAILED_CERT_VALIDATION_TIME_MOZILLAPKIX;
        break;
#ifndef NSS_NO_LIBPKIX
      case CertVerifier::libpkix:
        successTelemetry
          = Telemetry::SSL_SUCCESFUL_CERT_VALIDATION_TIME_LIBPKIX;
        failureTelemetry
          = Telemetry::SSL_INITIAL_FAILED_CERT_VALIDATION_TIME_LIBPKIX;
        break;
#endif
      default:
        MOZ_CRASH("Unknown CertVerifier mode");
    }

    // XXX
    // Reset the error code here so we can detect if AuthCertificate fails to
    // set the error code if/when it fails.
    PR_SetError(0, 0);
    SECStatus rv = AuthCertificate(*mCertVerifier, mInfoObject, mCert.get(),
                                   mStapledOCSPResponse, mProviderFlags,
                                   mTime);
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
          CreateCertErrorRunnable(*mCertVerifier, error, mInfoObject,
                                  mCert.get(), mStapledOCSPResponse,
                                  mFdForLogging, mProviderFlags, mTime));
      if (!runnable) {
        // CreateCertErrorRunnable set a new error code
        error = PR_GetError();
      } else {
        // We must block the the socket transport service thread while the
        // main thread executes the CertErrorRunnable. The CertErrorRunnable
        // will dispatch the result asynchronously, so we don't have to block
        // this thread waiting for it.

        PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
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

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
         ("[%p] starting AuthCertificateHook\n", fd));

  // Modern libssl always passes PR_TRUE for checkSig, and we have no means of
  // doing verification without checking signatures.
  NS_ASSERTION(checkSig, "AuthCertificateHook: checkSig unexpectedly false");

  // PSM never causes libssl to call this function with PR_TRUE for isServer,
  // and many things in PSM assume that we are a client.
  NS_ASSERTION(!isServer, "AuthCertificateHook: isServer unexpectedly true");

  nsNSSSocketInfo* socketInfo = static_cast<nsNSSSocketInfo*>(arg);

  ScopedCERTCertificate serverCert(SSL_PeerCertificate(fd));

  if (!checkSig || isServer || !socketInfo || !serverCert) {
      PR_SetError(PR_INVALID_STATE_ERROR, 0);
      return SECFailure;
  }

  socketInfo->SetFullHandshake();

  // This value of "now" is used both here for OCSP stapling and later
  // when calling CreateCertErrorRunnable.
  PRTime now = PR_Now();

  if (BlockServerCertChangeForSpdy(socketInfo, serverCert) != SECSuccess)
    return SECFailure;

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
                     serverCert, stapledOCSPResponse, providerFlags, now);
    return rv;
  }

  // We can't do certificate verification on a background thread, because the
  // thread doing the network I/O may not interrupt its network I/O on receipt
  // of our SSLServerCertVerificationResult event, and/or it might not even be
  // a non-blocking socket.

  SECStatus rv = AuthCertificate(*certVerifier, socketInfo, serverCert,
                                 stapledOCSPResponse, providerFlags, now);
  if (rv == SECSuccess) {
    Telemetry::Accumulate(Telemetry::SSL_CERT_ERROR_OVERRIDES, 1);
    return SECSuccess;
  }

  PRErrorCode error = PR_GetError();
  if (error != 0) {
    RefPtr<CertErrorRunnable> runnable(
        CreateCertErrorRunnable(*certVerifier, error, socketInfo, serverCert,
                                stapledOCSPResponse,
                                static_cast<const void*>(fd), providerFlags,
                                now));
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
  virtual nsresult CalculateResult() MOZ_OVERRIDE
  {
    EnsureIdentityInfoLoaded();
    return NS_OK;
  }

  virtual void ReleaseNSSResources() MOZ_OVERRIDE { } // no-op
  virtual void CallCallback(nsresult rv) MOZ_OVERRIDE { } // no-op
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
        TransportSecurityInfo* infoObject, PRErrorCode errorCode,
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

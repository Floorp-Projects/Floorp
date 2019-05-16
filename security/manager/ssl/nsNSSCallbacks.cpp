/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCallbacks.h"

#include "PSMRunnable.h"
#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"
#include "SharedSSLState.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Span.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsICertOverrideService.h"
#include "nsIHttpChannelInternal.h"
#include "nsIPrompt.h"
#include "nsIProtocolProxyService.h"
#include "nsISupportsPriority.h"
#include "nsIStreamLoader.h"
#include "nsITokenDialogs.h"
#include "nsIUploadChannel.h"
#include "nsIWebProgressListener.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsNSSIOLayer.h"
#include "nsNetUtil.h"
#include "nsProtectedAuthThread.h"
#include "nsProxyRelease.h"
#include "nsStringStream.h"
#include "mozpkix/pkixtypes.h"
#include "ssl.h"
#include "sslproto.h"

#include "TrustOverrideUtils.h"
#include "TrustOverride-SymantecData.inc"
#include "TrustOverride-AppleGoogleDigiCertData.inc"
#include "TrustOverride-TestImminentDistrustData.inc"

using namespace mozilla;
using namespace mozilla::pkix;
using namespace mozilla::psm;

extern LazyLogModule gPIPNSSLog;

static void AccumulateCipherSuite(Telemetry::HistogramID probe,
                                  const SSLChannelInfo& channelInfo);

namespace {

// Bits in bit mask for SSL_REASONS_FOR_NOT_FALSE_STARTING telemetry probe
// These bits are numbered so that the least subtle issues have higher values.
// This should make it easier for us to interpret the results.
const uint32_t POSSIBLE_VERSION_DOWNGRADE = 4;
const uint32_t POSSIBLE_CIPHER_SUITE_DOWNGRADE = 2;
const uint32_t KEA_NOT_SUPPORTED = 1;

}  // namespace

class OCSPRequest final : public nsIStreamLoaderObserver, public nsIRunnable {
 public:
  OCSPRequest(const nsACString& aiaLocation,
              const OriginAttributes& originAttributes,
              const uint8_t (&ocspRequest)[OCSP_REQUEST_MAX_LENGTH],
              size_t ocspRequestLength, TimeDuration timeout);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER
  NS_DECL_NSIRUNNABLE

  nsresult DispatchToMainThreadAndWait();
  nsresult GetResponse(/*out*/ Vector<uint8_t>& response);

 private:
  ~OCSPRequest() = default;

  static void OnTimeout(nsITimer* timer, void* closure);
  nsresult NotifyDone(nsresult rv, MonitorAutoLock& proofOfLock);

  // mMonitor provides the memory barrier protecting these member variables.
  // What happens is the originating thread creates an OCSPRequest object with
  // the information necessary to perform an OCSP request. It sends the object
  // to the main thread and waits on the monitor for the operation to complete.
  // On the main thread, a channel is set up to perform the request. This gets
  // dispatched to necko. At the same time, a timeout timer is initialized. If
  // the necko request completes, the response data is filled out, mNotifiedDone
  // is set to true, and the monitor is notified. The original thread then wakes
  // up and continues with the results that have been filled out. If the request
  // times out, again the response data is filled out, mNotifiedDone is set to
  // true, and the monitor is notified. The first of these two events wins. That
  // is, if the timeout timer fires but the request completes shortly after, the
  // caller will see the request as having timed out.
  // When the request completes (i.e. OnStreamComplete runs), the timer will be
  // cancelled. This is how we know the closure in OnTimeout is valid. If the
  // timer fires before OnStreamComplete runs, it should be safe to not cancel
  // the request because necko has a strong reference to it.
  Monitor mMonitor;
  bool mNotifiedDone;
  nsCOMPtr<nsIStreamLoader> mLoader;
  const nsCString mAIALocation;
  const OriginAttributes mOriginAttributes;
  const mozilla::Span<const char> mPOSTData;
  const TimeDuration mTimeout;
  nsCOMPtr<nsITimer> mTimeoutTimer;
  TimeStamp mStartTime;
  nsresult mResponseResult;
  Vector<uint8_t> mResponseBytes;
};

NS_IMPL_ISUPPORTS(OCSPRequest, nsIStreamLoaderObserver, nsIRunnable)

OCSPRequest::OCSPRequest(const nsACString& aiaLocation,
                         const OriginAttributes& originAttributes,
                         const uint8_t (&ocspRequest)[OCSP_REQUEST_MAX_LENGTH],
                         size_t ocspRequestLength, TimeDuration timeout)
    : mMonitor("OCSPRequest.mMonitor"),
      mNotifiedDone(false),
      mLoader(nullptr),
      mAIALocation(aiaLocation),
      mOriginAttributes(originAttributes),
      mPOSTData(reinterpret_cast<const char*>(ocspRequest), ocspRequestLength),
      mTimeout(timeout),
      mTimeoutTimer(nullptr),
      mStartTime(),
      mResponseResult(NS_ERROR_FAILURE),
      mResponseBytes() {
  MOZ_ASSERT(ocspRequestLength <= OCSP_REQUEST_MAX_LENGTH);
}

nsresult OCSPRequest::DispatchToMainThreadAndWait() {
  MOZ_ASSERT(!NS_IsMainThread());
  if (NS_IsMainThread()) {
    return NS_ERROR_FAILURE;
  }

  MonitorAutoLock lock(mMonitor);
  nsresult rv = NS_DispatchToMainThread(this);
  if (NS_FAILED(rv)) {
    return rv;
  }
  while (!mNotifiedDone) {
    lock.Wait();
  }

  TimeStamp endTime = TimeStamp::Now();
  // CERT_VALIDATION_HTTP_REQUEST_RESULT:
  // 0: request timed out
  // 1: request succeeded
  // 2: request failed
  // 3: internal error
  // If mStartTime was never set, we consider this an internal error.
  // Otherwise, we managed to at least send the request.
  if (mStartTime.IsNull()) {
    Telemetry::Accumulate(Telemetry::CERT_VALIDATION_HTTP_REQUEST_RESULT, 3);
  } else if (mResponseResult == NS_ERROR_NET_TIMEOUT) {
    Telemetry::Accumulate(Telemetry::CERT_VALIDATION_HTTP_REQUEST_RESULT, 0);
    Telemetry::AccumulateTimeDelta(
        Telemetry::CERT_VALIDATION_HTTP_REQUEST_CANCELED_TIME, mStartTime,
        endTime);
  } else if (NS_SUCCEEDED(mResponseResult)) {
    Telemetry::Accumulate(Telemetry::CERT_VALIDATION_HTTP_REQUEST_RESULT, 1);
    Telemetry::AccumulateTimeDelta(
        Telemetry::CERT_VALIDATION_HTTP_REQUEST_SUCCEEDED_TIME, mStartTime,
        endTime);
  } else {
    Telemetry::Accumulate(Telemetry::CERT_VALIDATION_HTTP_REQUEST_RESULT, 2);
    Telemetry::AccumulateTimeDelta(
        Telemetry::CERT_VALIDATION_HTTP_REQUEST_FAILED_TIME, mStartTime,
        endTime);
  }
  return rv;
}

nsresult OCSPRequest::GetResponse(/*out*/ Vector<uint8_t>& response) {
  MOZ_ASSERT(!NS_IsMainThread());
  if (NS_IsMainThread()) {
    return NS_ERROR_FAILURE;
  }

  MonitorAutoLock lock(mMonitor);
  if (!mNotifiedDone) {
    return NS_ERROR_IN_PROGRESS;
  }
  if (NS_FAILED(mResponseResult)) {
    return mResponseResult;
  }
  response.clear();
  if (!response.append(mResponseBytes.begin(), mResponseBytes.length())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

static NS_NAMED_LITERAL_CSTRING(OCSP_REQUEST_MIME_TYPE,
                                "application/ocsp-request");
static NS_NAMED_LITERAL_CSTRING(OCSP_REQUEST_METHOD, "POST");

NS_IMETHODIMP
OCSPRequest::Run() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_FAILURE;
  }

  MonitorAutoLock lock(mMonitor);

  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  if (!ios) {
    return NotifyDone(NS_ERROR_FAILURE, lock);
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv =
      NS_NewURI(getter_AddRefs(uri), mAIALocation, nullptr, nullptr, ios);
  if (NS_FAILED(rv)) {
    return NotifyDone(NS_ERROR_MALFORMED_URI, lock);
  }
  nsAutoCString scheme;
  rv = uri->GetScheme(scheme);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  if (!scheme.LowerCaseEqualsLiteral("http")) {
    return NotifyDone(NS_ERROR_MALFORMED_URI, lock);
  }

  // See bug 1219935.
  // We should not send OCSP request if the PAC is still loading.
  nsCOMPtr<nsIProtocolProxyService> pps =
      do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }

  if (pps->GetIsPACLoading()) {
    return NotifyDone(NS_ERROR_FAILURE, lock);
  }

  nsCOMPtr<nsIChannel> channel;
  rv = ios->NewChannel(mAIALocation, nullptr, nullptr,
                       nullptr,  // aLoadingNode
                       nsContentUtils::GetSystemPrincipal(),
                       nullptr,  // aTriggeringPrincipal
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER, getter_AddRefs(channel));
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }

  // Security operations scheduled through normal HTTP channels are given
  // high priority to accommodate real time OCSP transactions.
  nsCOMPtr<nsISupportsPriority> priorityChannel = do_QueryInterface(channel);
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_HIGHEST);
  }

  channel->SetLoadFlags(nsIRequest::LOAD_ANONYMOUS |
                        nsIChannel::LOAD_BYPASS_SERVICE_WORKER |
                        nsIChannel::LOAD_BYPASS_URL_CLASSIFIER);

  // For OCSP requests, only the first party domain and private browsing id
  // aspects of origin attributes are used. This means that:
  // a) if first party isolation is enabled, OCSP requests will be isolated
  // according to the first party domain of the original https request
  // b) OCSP requests are shared across different containers as long as first
  // party isolation is not enabled and none of the containers are in private
  // browsing mode.
  if (mOriginAttributes != OriginAttributes()) {
    OriginAttributes attrs;
    attrs.mFirstPartyDomain = mOriginAttributes.mFirstPartyDomain;
    attrs.mPrivateBrowsingId = mOriginAttributes.mPrivateBrowsingId;

    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    rv = loadInfo->SetOriginAttributes(attrs);
    if (NS_FAILED(rv)) {
      return NotifyDone(rv, lock);
    }
  }

  nsCOMPtr<nsIInputStream> uploadStream;
  rv = NS_NewByteInputStream(getter_AddRefs(uploadStream), mPOSTData,
                             NS_ASSIGNMENT_COPY);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(channel));
  if (!uploadChannel) {
    return NotifyDone(NS_ERROR_FAILURE, lock);
  }
  rv = uploadChannel->SetUploadStream(uploadStream, OCSP_REQUEST_MIME_TYPE, -1);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  // Do not use SPDY for internal security operations. It could result
  // in the silent upgrade to ssl, which in turn could require an SSL
  // operation to fulfill something like an OCSP fetch, which is an
  // endless loop.
  nsCOMPtr<nsIHttpChannelInternal> internalChannel = do_QueryInterface(channel);
  if (!internalChannel) {
    return NotifyDone(rv, lock);
  }
  rv = internalChannel->SetAllowSpdy(false);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  nsCOMPtr<nsIHttpChannel> hchan = do_QueryInterface(channel);
  if (!hchan) {
    return NotifyDone(NS_ERROR_FAILURE, lock);
  }
  rv = hchan->SetAllowSTS(false);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  rv = hchan->SetRequestMethod(OCSP_REQUEST_METHOD);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }

  rv = NS_NewStreamLoader(getter_AddRefs(mLoader), this);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }

  rv = NS_NewTimerWithFuncCallback(
      getter_AddRefs(mTimeoutTimer), OCSPRequest::OnTimeout, this,
      mTimeout.ToMilliseconds(), nsITimer::TYPE_ONE_SHOT, "OCSPRequest::Run");
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  rv = hchan->AsyncOpen(this->mLoader);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  mStartTime = TimeStamp::Now();
  return NS_OK;
}

nsresult OCSPRequest::NotifyDone(nsresult rv, MonitorAutoLock& lock) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_FAILURE;
  }

  if (mNotifiedDone) {
    return mResponseResult;
  }
  mLoader = nullptr;
  mResponseResult = rv;
  if (mTimeoutTimer) {
    Unused << mTimeoutTimer->Cancel();
  }
  mNotifiedDone = true;
  lock.Notify();
  return rv;
}

NS_IMETHODIMP
OCSPRequest::OnStreamComplete(nsIStreamLoader* aLoader, nsISupports* aContext,
                              nsresult aStatus, uint32_t responseLen,
                              const uint8_t* responseBytes) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return NS_ERROR_FAILURE;
  }

  MonitorAutoLock lock(mMonitor);

  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }

  if (NS_FAILED(aStatus)) {
    return NotifyDone(aStatus, lock);
  }

  nsCOMPtr<nsIHttpChannel> hchan = do_QueryInterface(req);
  if (!hchan) {
    return NotifyDone(NS_ERROR_FAILURE, lock);
  }

  bool requestSucceeded;
  rv = hchan->GetRequestSucceeded(&requestSucceeded);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  if (!requestSucceeded) {
    return NotifyDone(NS_ERROR_FAILURE, lock);
  }

  unsigned int rcode;
  rv = hchan->GetResponseStatus(&rcode);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  if (rcode != 200) {
    return NotifyDone(NS_ERROR_FAILURE, lock);
  }

  mResponseBytes.clear();
  if (!mResponseBytes.append(responseBytes, responseLen)) {
    return NotifyDone(NS_ERROR_OUT_OF_MEMORY, lock);
  }
  mResponseResult = aStatus;

  return NotifyDone(NS_OK, lock);
}

void OCSPRequest::OnTimeout(nsITimer* timer, void* closure) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }

  // We know the OCSPRequest is still alive because if the request had completed
  // (i.e. OnStreamComplete ran), the timer would have been cancelled in
  // NotifyDone.
  OCSPRequest* self = static_cast<OCSPRequest*>(closure);
  MonitorAutoLock lock(self->mMonitor);
  self->mTimeoutTimer = nullptr;
  self->NotifyDone(NS_ERROR_NET_TIMEOUT, lock);
}

mozilla::pkix::Result DoOCSPRequest(
    const nsCString& aiaLocation, const OriginAttributes& originAttributes,
    uint8_t (&ocspRequest)[OCSP_REQUEST_MAX_LENGTH], size_t ocspRequestLength,
    TimeDuration timeout, /*out*/ Vector<uint8_t>& result) {
  MOZ_ASSERT(!NS_IsMainThread());
  if (NS_IsMainThread()) {
    return mozilla::pkix::Result::ERROR_OCSP_UNKNOWN_CERT;
  }

  if (ocspRequestLength > OCSP_REQUEST_MAX_LENGTH) {
    return mozilla::pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  result.clear();
  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("DoOCSPRequest to '%s'", aiaLocation.get()));

  nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(sts);
  if (!sts) {
    return mozilla::pkix::Result::FATAL_ERROR_INVALID_STATE;
  }
  bool onSTSThread;
  nsresult rv = sts->IsOnCurrentThread(&onSTSThread);
  if (NS_FAILED(rv)) {
    return mozilla::pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  MOZ_ASSERT(!onSTSThread);
  if (onSTSThread) {
    return mozilla::pkix::Result::FATAL_ERROR_INVALID_STATE;
  }

  RefPtr<OCSPRequest> request(new OCSPRequest(
      aiaLocation, originAttributes, ocspRequest, ocspRequestLength, timeout));
  rv = request->DispatchToMainThreadAndWait();
  if (NS_FAILED(rv)) {
    return mozilla::pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  rv = request->GetResponse(result);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_MALFORMED_URI) {
      return mozilla::pkix::Result::ERROR_CERT_BAD_ACCESS_LOCATION;
    }
    return mozilla::pkix::Result::ERROR_OCSP_SERVER_ERROR;
  }
  return Success;
}

static char* ShowProtectedAuthPrompt(PK11SlotInfo* slot,
                                     nsIInterfaceRequestor* ir) {
  if (!NS_IsMainThread()) {
    NS_ERROR("ShowProtectedAuthPrompt called off the main thread");
    return nullptr;
  }

  char* protAuthRetVal = nullptr;

  // Get protected auth dialogs
  nsCOMPtr<nsITokenDialogs> dialogs;
  nsresult nsrv =
      getNSSDialogs(getter_AddRefs(dialogs), NS_GET_IID(nsITokenDialogs),
                    NS_TOKENDIALOGS_CONTRACTID);
  if (NS_SUCCEEDED(nsrv)) {
    RefPtr<nsProtectedAuthThread> protectedAuthRunnable =
        new nsProtectedAuthThread();
    protectedAuthRunnable->SetParams(slot);

    nsrv = dialogs->DisplayProtectedAuth(ir, protectedAuthRunnable);

    // We call join on the thread,
    // so we can be sure that no simultaneous access will happen.
    protectedAuthRunnable->Join();

    if (NS_SUCCEEDED(nsrv)) {
      SECStatus rv = protectedAuthRunnable->GetResult();
      switch (rv) {
        case SECSuccess:
          protAuthRetVal =
              ToNewCString(nsDependentCString(PK11_PW_AUTHENTICATED));
          break;
        case SECWouldBlock:
          protAuthRetVal = ToNewCString(nsDependentCString(PK11_PW_RETRY));
          break;
        default:
          protAuthRetVal = nullptr;
          break;
      }
    }
  }

  return protAuthRetVal;
}

class PK11PasswordPromptRunnable : public SyncRunnableBase {
 public:
  PK11PasswordPromptRunnable(PK11SlotInfo* slot, nsIInterfaceRequestor* ir)
      : mResult(nullptr), mSlot(slot), mIR(ir) {}
  virtual ~PK11PasswordPromptRunnable() = default;

  char* mResult;  // out
  virtual void RunOnTargetThread() override;

 private:
  PK11SlotInfo* const mSlot;         // in
  nsIInterfaceRequestor* const mIR;  // in
};

void PK11PasswordPromptRunnable::RunOnTargetThread() {
  nsresult rv;
  nsCOMPtr<nsIPrompt> prompt;
  if (!mIR) {
    rv = nsNSSComponent::GetNewPrompter(getter_AddRefs(prompt));
    if (NS_FAILED(rv)) {
      return;
    }
  } else {
    prompt = do_GetInterface(mIR);
    MOZ_ASSERT(prompt, "Interface requestor should implement nsIPrompt");
  }

  if (!prompt) {
    return;
  }

  if (PK11_ProtectedAuthenticationPath(mSlot)) {
    mResult = ShowProtectedAuthPrompt(mSlot, mIR);
    return;
  }

  nsAutoString promptString;
  if (PK11_IsInternal(mSlot)) {
    rv = GetPIPNSSBundleString("CertPassPromptDefault", promptString);
  } else {
    NS_ConvertUTF8toUTF16 tokenName(PK11_GetTokenName(mSlot));
    const char16_t* formatStrings[] = {
        tokenName.get(),
    };
    rv =
        PIPBundleFormatStringFromName("CertPassPrompt", formatStrings,
                                      ArrayLength(formatStrings), promptString);
  }
  if (NS_FAILED(rv)) {
    return;
  }

  nsString password;
  // |checkState| is unused because |checkMsg| (the argument just before it) is
  // null, but XPConnect requires it to point to a valid bool nonetheless.
  bool checkState = false;
  bool userClickedOK = false;
  rv = prompt->PromptPassword(nullptr, promptString.get(),
                              getter_Copies(password), nullptr, &checkState,
                              &userClickedOK);
  if (NS_FAILED(rv) || !userClickedOK) {
    return;
  }

  mResult = ToNewUTF8String(password);
}

char* PK11PasswordPrompt(PK11SlotInfo* slot, PRBool /*retry*/, void* arg) {
  RefPtr<PK11PasswordPromptRunnable> runnable(new PK11PasswordPromptRunnable(
      slot, static_cast<nsIInterfaceRequestor*>(arg)));
  runnable->DispatchToMainThreadAndWait();
  return runnable->mResult;
}

static nsCString getKeaGroupName(uint32_t aKeaGroup) {
  nsCString groupName;
  switch (aKeaGroup) {
    case ssl_grp_ec_secp256r1:
      groupName = NS_LITERAL_CSTRING("P256");
      break;
    case ssl_grp_ec_secp384r1:
      groupName = NS_LITERAL_CSTRING("P384");
      break;
    case ssl_grp_ec_secp521r1:
      groupName = NS_LITERAL_CSTRING("P521");
      break;
    case ssl_grp_ec_curve25519:
      groupName = NS_LITERAL_CSTRING("x25519");
      break;
    case ssl_grp_ffdhe_2048:
      groupName = NS_LITERAL_CSTRING("FF 2048");
      break;
    case ssl_grp_ffdhe_3072:
      groupName = NS_LITERAL_CSTRING("FF 3072");
      break;
    case ssl_grp_none:
      groupName = NS_LITERAL_CSTRING("none");
      break;
    case ssl_grp_ffdhe_custom:
      groupName = NS_LITERAL_CSTRING("custom");
      break;
    // All other groups are not enabled in Firefox. See namedGroups in
    // nsNSSIOLayer.cpp.
    default:
      // This really shouldn't happen!
      MOZ_ASSERT_UNREACHABLE("Invalid key exchange group.");
      groupName = NS_LITERAL_CSTRING("unknown group");
  }
  return groupName;
}

static nsCString getSignatureName(uint32_t aSignatureScheme) {
  nsCString signatureName;
  switch (aSignatureScheme) {
    case ssl_sig_none:
      signatureName = NS_LITERAL_CSTRING("none");
      break;
    case ssl_sig_rsa_pkcs1_sha1:
      signatureName = NS_LITERAL_CSTRING("RSA-PKCS1-SHA1");
      break;
    case ssl_sig_rsa_pkcs1_sha256:
      signatureName = NS_LITERAL_CSTRING("RSA-PKCS1-SHA256");
      break;
    case ssl_sig_rsa_pkcs1_sha384:
      signatureName = NS_LITERAL_CSTRING("RSA-PKCS1-SHA384");
      break;
    case ssl_sig_rsa_pkcs1_sha512:
      signatureName = NS_LITERAL_CSTRING("RSA-PKCS1-SHA512");
      break;
    case ssl_sig_ecdsa_secp256r1_sha256:
      signatureName = NS_LITERAL_CSTRING("ECDSA-P256-SHA256");
      break;
    case ssl_sig_ecdsa_secp384r1_sha384:
      signatureName = NS_LITERAL_CSTRING("ECDSA-P384-SHA384");
      break;
    case ssl_sig_ecdsa_secp521r1_sha512:
      signatureName = NS_LITERAL_CSTRING("ECDSA-P521-SHA512");
      break;
    case ssl_sig_rsa_pss_sha256:
      signatureName = NS_LITERAL_CSTRING("RSA-PSS-SHA256");
      break;
    case ssl_sig_rsa_pss_sha384:
      signatureName = NS_LITERAL_CSTRING("RSA-PSS-SHA384");
      break;
    case ssl_sig_rsa_pss_sha512:
      signatureName = NS_LITERAL_CSTRING("RSA-PSS-SHA512");
      break;
    case ssl_sig_ecdsa_sha1:
      signatureName = NS_LITERAL_CSTRING("ECDSA-SHA1");
      break;
    case ssl_sig_rsa_pkcs1_sha1md5:
      signatureName = NS_LITERAL_CSTRING("RSA-PKCS1-SHA1MD5");
      break;
    // All other groups are not enabled in Firefox. See sEnabledSignatureSchemes
    // in nsNSSIOLayer.cpp.
    default:
      // This really shouldn't happen!
      MOZ_ASSERT_UNREACHABLE("Invalid signature scheme.");
      signatureName = NS_LITERAL_CSTRING("unknown signature");
  }
  return signatureName;
}

// call with shutdown prevention lock held
static void PreliminaryHandshakeDone(PRFileDesc* fd) {
  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*)fd->higher->secret;
  if (!infoObject) return;

  SSLChannelInfo channelInfo;
  if (SSL_GetChannelInfo(fd, &channelInfo, sizeof(channelInfo)) == SECSuccess) {
    infoObject->SetSSLVersionUsed(channelInfo.protocolVersion);
    infoObject->SetEarlyDataAccepted(channelInfo.earlyDataAccepted);
    infoObject->SetResumed(channelInfo.resumed);

    SSLCipherSuiteInfo cipherInfo;
    if (SSL_GetCipherSuiteInfo(channelInfo.cipherSuite, &cipherInfo,
                               sizeof cipherInfo) == SECSuccess) {
      /* Set the Status information */
      infoObject->mHaveCipherSuiteAndProtocol = true;
      infoObject->mCipherSuite = channelInfo.cipherSuite;
      infoObject->mProtocolVersion = channelInfo.protocolVersion & 0xFF;
      infoObject->mKeaGroup.Assign(getKeaGroupName(channelInfo.keaGroup));
      infoObject->mSignatureSchemeName.Assign(
          getSignatureName(channelInfo.signatureScheme));
      infoObject->SetKEAUsed(channelInfo.keaType);
      infoObject->SetKEAKeyBits(channelInfo.keaKeyBits);
      infoObject->SetMACAlgorithmUsed(cipherInfo.macAlgorithm);
    }
  }

  // Don't update NPN details on renegotiation.
  if (infoObject->IsPreliminaryHandshakeDone()) {
    return;
  }

  // Get the NPN value.
  SSLNextProtoState state;
  unsigned char npnbuf[256];
  unsigned int npnlen;

  if (SSL_GetNextProto(fd, &state, npnbuf, &npnlen,
                       AssertedCast<unsigned int>(ArrayLength(npnbuf))) ==
      SECSuccess) {
    if (state == SSL_NEXT_PROTO_NEGOTIATED ||
        state == SSL_NEXT_PROTO_SELECTED) {
      infoObject->SetNegotiatedNPN(BitwiseCast<char*, unsigned char*>(npnbuf),
                                   npnlen);
    } else {
      infoObject->SetNegotiatedNPN(nullptr, 0);
    }
    mozilla::Telemetry::Accumulate(Telemetry::SSL_NPN_TYPE, state);
  } else {
    infoObject->SetNegotiatedNPN(nullptr, 0);
  }

  infoObject->SetPreliminaryHandshakeDone();
}

SECStatus CanFalseStartCallback(PRFileDesc* fd, void* client_data,
                                PRBool* canFalseStart) {
  *canFalseStart = false;

  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*)fd->higher->secret;
  if (!infoObject) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  infoObject->SetFalseStartCallbackCalled();

  PreliminaryHandshakeDone(fd);

  uint32_t reasonsForNotFalseStarting = 0;

  SSLChannelInfo channelInfo;
  if (SSL_GetChannelInfo(fd, &channelInfo, sizeof(channelInfo)) != SECSuccess) {
    return SECSuccess;
  }

  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(channelInfo.cipherSuite, &cipherInfo,
                             sizeof(cipherInfo)) != SECSuccess) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("CanFalseStartCallback [%p] failed - "
             " KEA %d\n",
             fd, static_cast<int32_t>(channelInfo.keaType)));
    return SECSuccess;
  }

  // Prevent version downgrade attacks from TLS 1.2, and avoid False Start for
  // TLS 1.3 and later. See Bug 861310 for all the details as to why.
  if (channelInfo.protocolVersion != SSL_LIBRARY_VERSION_TLS_1_2) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("CanFalseStartCallback [%p] failed - "
             "SSL Version must be TLS 1.2, was %x\n",
             fd, static_cast<int32_t>(channelInfo.protocolVersion)));
    reasonsForNotFalseStarting |= POSSIBLE_VERSION_DOWNGRADE;
  }

  // See bug 952863 for why ECDHE is allowed, but DHE (and RSA) are not.
  if (channelInfo.keaType != ssl_kea_ecdh) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("CanFalseStartCallback [%p] failed - "
             "unsupported KEA %d\n",
             fd, static_cast<int32_t>(channelInfo.keaType)));
    reasonsForNotFalseStarting |= KEA_NOT_SUPPORTED;
  }

  // Prevent downgrade attacks on the symmetric cipher. We do not allow CBC
  // mode due to BEAST, POODLE, and other attacks on the MAC-then-Encrypt
  // design. See bug 1109766 for more details.
  if (cipherInfo.macAlgorithm != ssl_mac_aead) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("CanFalseStartCallback [%p] failed - non-AEAD cipher used, %d, "
             "is not supported with False Start.\n",
             fd, static_cast<int32_t>(cipherInfo.symCipher)));
    reasonsForNotFalseStarting |= POSSIBLE_CIPHER_SUITE_DOWNGRADE;
  }

  // XXX: An attacker can choose which protocols are advertised in the
  // NPN extension. TODO(Bug 861311): We should restrict the ability
  // of an attacker leverage this capability by restricting false start
  // to the same protocol we previously saw for the server, after the
  // first successful connection to the server.

  Telemetry::Accumulate(Telemetry::SSL_REASONS_FOR_NOT_FALSE_STARTING,
                        reasonsForNotFalseStarting);

  if (reasonsForNotFalseStarting == 0) {
    *canFalseStart = PR_TRUE;
    infoObject->SetFalseStarted();
    infoObject->NoteTimeUntilReady();
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("CanFalseStartCallback [%p] ok\n", fd));
  }

  return SECSuccess;
}

static void AccumulateNonECCKeySize(Telemetry::HistogramID probe,
                                    uint32_t bits) {
  unsigned int value =
      bits < 512
          ? 1
          : bits == 512
                ? 2
                : bits < 768
                      ? 3
                      : bits == 768
                            ? 4
                            : bits < 1024
                                  ? 5
                                  : bits == 1024
                                        ? 6
                                        : bits < 1280
                                              ? 7
                                              : bits == 1280
                                                    ? 8
                                                    : bits < 1536
                                                          ? 9
                                                          : bits == 1536
                                                                ? 10
                                                                : bits < 2048
                                                                      ? 11
                                                                      : bits == 2048
                                                                            ? 12
                                                                            : bits < 3072
                                                                                  ? 13
                                                                                  : bits == 3072
                                                                                        ? 14
                                                                                        : bits < 4096
                                                                                              ? 15
                                                                                              : bits == 4096
                                                                                                    ? 16
                                                                                                    : bits < 8192
                                                                                                          ? 17
                                                                                                          : bits == 8192
                                                                                                                ? 18
                                                                                                                : bits < 16384
                                                                                                                      ? 19
                                                                                                                      : bits == 16384
                                                                                                                            ? 20
                                                                                                                            : 0;
  Telemetry::Accumulate(probe, value);
}

// XXX: This attempts to map a bit count to an ECC named curve identifier. In
// the vast majority of situations, we only have the Suite B curves available.
// In that case, this mapping works fine. If we were to have more curves
// available, the mapping would be ambiguous since there could be multiple
// named curves for a given size (e.g. secp256k1 vs. secp256r1). We punt on
// that for now. See also NSS bug 323674.
static void AccumulateECCCurve(Telemetry::HistogramID probe, uint32_t bits) {
  unsigned int value = bits == 256 ? 23                              // P-256
                                   : bits == 384 ? 24                // P-384
                                                 : bits == 521 ? 25  // P-521
                                                               : 0;  // Unknown
  Telemetry::Accumulate(probe, value);
}

static void AccumulateCipherSuite(Telemetry::HistogramID probe,
                                  const SSLChannelInfo& channelInfo) {
  uint32_t value;
  switch (channelInfo.cipherSuite) {
    // ECDHE key exchange
    case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
      value = 1;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
      value = 2;
      break;
    case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
      value = 3;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
      value = 4;
      break;
    case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
      value = 5;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
      value = 6;
      break;
    case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:
      value = 7;
      break;
    case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:
      value = 10;
      break;
    case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
      value = 11;
      break;
    case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
      value = 12;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
      value = 13;
      break;
    case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
      value = 14;
      break;
    // DHE key exchange
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
      value = 21;
      break;
    case TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA:
      value = 22;
      break;
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
      value = 23;
      break;
    case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
      value = 24;
      break;
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
      value = 25;
      break;
    case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
      value = 26;
      break;
    case TLS_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA:
      value = 27;
      break;
    case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
      value = 28;
      break;
    case TLS_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA:
      value = 29;
      break;
    case TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
      value = 30;
      break;
    // ECDH key exchange
    case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:
      value = 41;
      break;
    case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:
      value = 42;
      break;
    case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:
      value = 43;
      break;
    case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:
      value = 44;
      break;
    case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:
      value = 45;
      break;
    case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:
      value = 46;
      break;
    // RSA key exchange
    case TLS_RSA_WITH_AES_128_CBC_SHA:
      value = 61;
      break;
    case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
      value = 62;
      break;
    case TLS_RSA_WITH_AES_256_CBC_SHA:
      value = 63;
      break;
    case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
      value = 64;
      break;
    case SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA:
      value = 65;
      break;
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
      value = 66;
      break;
    case TLS_RSA_WITH_SEED_CBC_SHA:
      value = 67;
      break;
    // TLS 1.3 PSK resumption
    case TLS_AES_128_GCM_SHA256:
      value = 70;
      break;
    case TLS_CHACHA20_POLY1305_SHA256:
      value = 71;
      break;
    case TLS_AES_256_GCM_SHA384:
      value = 72;
      break;
    // unknown
    default:
      value = 0;
      break;
  }
  MOZ_ASSERT(value != 0);
  Telemetry::Accumulate(probe, value);
}

// In the case of session resumption, the AuthCertificate hook has been bypassed
// (because we've previously successfully connected to our peer). That being the
// case, we unfortunately don't know what the verified certificate chain was, if
// the peer's server certificate verified as extended validation, or what its CT
// status is (if enabled). To address this, we attempt to build a certificate
// chain here using as much of the original context as possible (e.g. stapled
// OCSP responses, SCTs, the hostname, the first party domain, etc.). Note that
// because we are on the socket thread, this must not cause any network
// requests, hence the use of FLAG_LOCAL_ONLY.
static void RebuildVerifiedCertificateInformation(PRFileDesc* fd,
                                                  nsNSSSocketInfo* infoObject) {
  MOZ_ASSERT(fd);
  MOZ_ASSERT(infoObject);

  if (!fd || !infoObject) {
    return;
  }

  UniqueCERTCertificate cert(SSL_PeerCertificate(fd));
  MOZ_ASSERT(cert, "SSL_PeerCertificate failed in TLS handshake callback?");
  if (!cert) {
    return;
  }

  RefPtr<SharedCertVerifier> certVerifier(GetDefaultCertVerifier());
  MOZ_ASSERT(certVerifier,
             "Certificate verifier uninitialized in TLS handshake callback?");
  if (!certVerifier) {
    return;
  }

  // We don't own these pointers.
  const SECItemArray* stapledOCSPResponses = SSL_PeerStapledOCSPResponses(fd);
  const SECItem* stapledOCSPResponse = nullptr;
  // we currently only support single stapled responses
  if (stapledOCSPResponses && stapledOCSPResponses->len == 1) {
    stapledOCSPResponse = &stapledOCSPResponses->items[0];
  }
  const SECItem* sctsFromTLSExtension = SSL_PeerSignedCertTimestamps(fd);
  if (sctsFromTLSExtension && sctsFromTLSExtension->len == 0) {
    // SSL_PeerSignedCertTimestamps returns null on error and empty item
    // when no extension was returned by the server. We always use null when
    // no extension was received (for whatever reason), ignoring errors.
    sctsFromTLSExtension = nullptr;
  }

  int flags = mozilla::psm::CertVerifier::FLAG_LOCAL_ONLY;
  if (!infoObject->SharedState().IsOCSPStaplingEnabled() ||
      !infoObject->SharedState().IsOCSPMustStapleEnabled()) {
    flags |= CertVerifier::FLAG_TLS_IGNORE_STATUS_REQUEST;
  }

  SECOidTag evOidPolicy;
  CertificateTransparencyInfo certificateTransparencyInfo;
  UniqueCERTCertList builtChain;
  const bool saveIntermediates = false;
  mozilla::pkix::Result rv = certVerifier->VerifySSLServerCert(
      cert, stapledOCSPResponse, sctsFromTLSExtension, mozilla::pkix::Now(),
      infoObject, infoObject->GetHostName(), builtChain, saveIntermediates,
      flags, infoObject->GetOriginAttributes(), &evOidPolicy,
      nullptr,  // OCSP stapling telemetry
      nullptr,  // key size telemetry
      nullptr,  // SHA-1 telemetry
      nullptr,  // pinning telemetry
      &certificateTransparencyInfo);

  if (rv != Success) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("HandshakeCallback: couldn't rebuild verified certificate info"));
  }

  RefPtr<nsNSSCertificate> nssc(nsNSSCertificate::Create(cert.get()));
  if (rv == Success && evOidPolicy != SEC_OID_UNKNOWN) {
    infoObject->SetCertificateTransparencyInfo(certificateTransparencyInfo);
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("HandshakeCallback using NEW cert %p (is EV)", nssc.get()));
    infoObject->SetServerCert(nssc, EVStatus::EV);
  } else {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("HandshakeCallback using NEW cert %p (is not EV)", nssc.get()));
    infoObject->SetServerCert(nssc, EVStatus::NotEV);
  }

  if (rv == Success) {
    infoObject->SetCertificateTransparencyInfo(certificateTransparencyInfo);
    infoObject->SetSucceededCertChain(std::move(builtChain));
  }
}

static nsresult IsCertificateDistrustImminent(nsIX509CertList* aCertList,
                                              /* out */ bool& isDistrusted) {
  if (!aCertList) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsCOMPtr<nsIX509Cert> rootCert;
  nsCOMPtr<nsIX509CertList> intCerts;
  nsCOMPtr<nsIX509Cert> eeCert;

  RefPtr<nsNSSCertList> certList = aCertList->GetCertList();
  nsresult rv = certList->SegmentCertificateChain(rootCert, intCerts, eeCert);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Check the test certificate condition first; this is a special certificate
  // that gets the 'imminent distrust' treatment; this is so that the distrust
  // UX code does not become stale, as it will need regular use. See Bug 1409257
  // for context. Please do not remove this when adjusting the rest of the
  // method.
  UniqueCERTCertificate nssEECert(eeCert->GetCert());
  if (!nssEECert) {
    return NS_ERROR_FAILURE;
  }
  isDistrusted =
      CertDNIsInList(nssEECert.get(), TestImminentDistrustEndEntityDNs);
  if (isDistrusted) {
    // Exit early
    return NS_OK;
  }

  UniqueCERTCertificate nssRootCert(rootCert->GetCert());
  if (!nssRootCert) {
    return NS_ERROR_FAILURE;
  }

  // Proceed with the Symantec imminent distrust algorithm. This algorithm is
  // to be removed in Firefox 63, when the validity period check will also be
  // removed from the code in NSSCertDBTrustDomain.
  if (CertDNIsInList(nssRootCert.get(), RootSymantecDNs)) {
    static const PRTime NULL_TIME = 0;

    rv = CheckForSymantecDistrust(intCerts, eeCert, NULL_TIME,
                                  RootAppleAndGoogleSPKIs, isDistrusted);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

void HandshakeCallback(PRFileDesc* fd, void* client_data) {
  SECStatus rv;

  nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*)fd->higher->secret;

  // Do the bookkeeping that needs to be done after the
  // server's ServerHello...ServerHelloDone have been processed, but that
  // doesn't need the handshake to be completed.
  PreliminaryHandshakeDone(fd);

  nsSSLIOLayerHelpers& ioLayerHelpers =
      infoObject->SharedState().IOLayerHelpers();

  SSLVersionRange versions(infoObject->GetTLSVersionRange());

  MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
          ("[%p] HandshakeCallback: succeeded using TLS version range "
           "(0x%04x,0x%04x)\n",
           fd, static_cast<unsigned int>(versions.min),
           static_cast<unsigned int>(versions.max)));

  // If the handshake completed, then we know the site is TLS tolerant
  ioLayerHelpers.rememberTolerantAtVersion(infoObject->GetHostName(),
                                           infoObject->GetPort(), versions.max);

  SSLChannelInfo channelInfo;
  rv = SSL_GetChannelInfo(fd, &channelInfo, sizeof(channelInfo));
  MOZ_ASSERT(rv == SECSuccess);
  if (rv == SECSuccess) {
    // Get the protocol version for telemetry
    // 1=tls1, 2=tls1.1, 3=tls1.2
    unsigned int versionEnum = channelInfo.protocolVersion & 0xFF;
    MOZ_ASSERT(versionEnum > 0);
    Telemetry::Accumulate(Telemetry::SSL_HANDSHAKE_VERSION, versionEnum);
    AccumulateCipherSuite(infoObject->IsFullHandshake()
                              ? Telemetry::SSL_CIPHER_SUITE_FULL
                              : Telemetry::SSL_CIPHER_SUITE_RESUMED,
                          channelInfo);

    SSLCipherSuiteInfo cipherInfo;
    rv = SSL_GetCipherSuiteInfo(channelInfo.cipherSuite, &cipherInfo,
                                sizeof cipherInfo);
    MOZ_ASSERT(rv == SECSuccess);
    if (rv == SECSuccess) {
      // keyExchange null=0, rsa=1, dh=2, fortezza=3, ecdh=4
      Telemetry::Accumulate(infoObject->IsFullHandshake()
                                ? Telemetry::SSL_KEY_EXCHANGE_ALGORITHM_FULL
                                : Telemetry::SSL_KEY_EXCHANGE_ALGORITHM_RESUMED,
                            channelInfo.keaType);

      MOZ_ASSERT(infoObject->GetKEAUsed() == channelInfo.keaType);

      if (infoObject->IsFullHandshake()) {
        switch (channelInfo.keaType) {
          case ssl_kea_rsa:
            AccumulateNonECCKeySize(Telemetry::SSL_KEA_RSA_KEY_SIZE_FULL,
                                    channelInfo.keaKeyBits);
            break;
          case ssl_kea_dh:
            AccumulateNonECCKeySize(Telemetry::SSL_KEA_DHE_KEY_SIZE_FULL,
                                    channelInfo.keaKeyBits);
            break;
          case ssl_kea_ecdh:
            AccumulateECCCurve(Telemetry::SSL_KEA_ECDHE_CURVE_FULL,
                               channelInfo.keaKeyBits);
            break;
          default:
            MOZ_CRASH("impossible KEA");
            break;
        }

        Telemetry::Accumulate(Telemetry::SSL_AUTH_ALGORITHM_FULL,
                              channelInfo.authType);

        // RSA key exchange doesn't use a signature for auth.
        if (channelInfo.keaType != ssl_kea_rsa) {
          switch (channelInfo.authType) {
            case ssl_auth_rsa:
            case ssl_auth_rsa_sign:
              AccumulateNonECCKeySize(Telemetry::SSL_AUTH_RSA_KEY_SIZE_FULL,
                                      channelInfo.authKeyBits);
              break;
            case ssl_auth_ecdsa:
              AccumulateECCCurve(Telemetry::SSL_AUTH_ECDSA_CURVE_FULL,
                                 channelInfo.authKeyBits);
              break;
            default:
              MOZ_CRASH("impossible auth algorithm");
              break;
          }
        }
      }

      Telemetry::Accumulate(infoObject->IsFullHandshake()
                                ? Telemetry::SSL_SYMMETRIC_CIPHER_FULL
                                : Telemetry::SSL_SYMMETRIC_CIPHER_RESUMED,
                            cipherInfo.symCipher);
    }
  }

  PRBool siteSupportsSafeRenego;
  if (channelInfo.protocolVersion != SSL_LIBRARY_VERSION_TLS_1_3) {
    rv = SSL_HandshakeNegotiatedExtension(fd, ssl_renegotiation_info_xtn,
                                          &siteSupportsSafeRenego);
    MOZ_ASSERT(rv == SECSuccess);
    if (rv != SECSuccess) {
      siteSupportsSafeRenego = false;
    }
  } else {
    // TLS 1.3 dropped support for renegotiation.
    siteSupportsSafeRenego = true;
  }
  bool renegotiationUnsafe = !siteSupportsSafeRenego &&
                             ioLayerHelpers.treatUnsafeNegotiationAsBroken();

  bool deprecatedTlsVer =
      (channelInfo.protocolVersion < SSL_LIBRARY_VERSION_TLS_1_2);
  RememberCertErrorsTable::GetInstance().LookupCertErrorBits(infoObject);

  uint32_t state;
  if (renegotiationUnsafe || deprecatedTlsVer) {
    state = nsIWebProgressListener::STATE_IS_BROKEN;
  } else {
    state = nsIWebProgressListener::STATE_IS_SECURE;
    SSLVersionRange defVersion;
    rv = SSL_VersionRangeGetDefault(ssl_variant_stream, &defVersion);
    if (rv == SECSuccess && versions.max >= defVersion.max) {
      // we know this site no longer requires a version fallback
      ioLayerHelpers.removeInsecureFallbackSite(infoObject->GetHostName(),
                                                infoObject->GetPort());
    }
  }

  if (infoObject->HasServerCert()) {
    MOZ_LOG(gPIPNSSLog, LogLevel::Debug,
            ("HandshakeCallback KEEPING existing cert\n"));
  } else {
    RebuildVerifiedCertificateInformation(fd, infoObject);
  }

  nsCOMPtr<nsIX509CertList> succeededCertChain;
  // This always returns NS_OK, but the list could be empty. This is a
  // best-effort check for now. Bug 731478 will reduce the incidence of empty
  // succeeded cert chains through better caching.
  Unused << infoObject->GetSucceededCertChain(
      getter_AddRefs(succeededCertChain));
  bool distrustImminent;
  nsresult srv =
      IsCertificateDistrustImminent(succeededCertChain, distrustImminent);
  if (NS_SUCCEEDED(srv) && distrustImminent) {
    state |= nsIWebProgressListener::STATE_CERT_DISTRUST_IMMINENT;
  }

  bool domainMismatch;
  bool untrusted;
  bool notValidAtThisTime;
  // These all return NS_OK, so don't even bother checking the return values.
  Unused << infoObject->GetIsDomainMismatch(&domainMismatch);
  Unused << infoObject->GetIsUntrusted(&untrusted);
  Unused << infoObject->GetIsNotValidAtThisTime(&notValidAtThisTime);
  // If we're here, the TLS handshake has succeeded. Thus if any of these
  // booleans are true, the user has added an override for a certificate error.
  if (domainMismatch || untrusted || notValidAtThisTime) {
    state |= nsIWebProgressListener::STATE_CERT_USER_OVERRIDDEN;
  }

  infoObject->SetSecurityState(state);

  // XXX Bug 883674: We shouldn't be formatting messages here in PSM; instead,
  // we should set a flag on the channel that higher (UI) level code can check
  // to log the warning. In particular, these warnings should go to the web
  // console instead of to the error console. Also, the warning is not
  // localized.
  if (!siteSupportsSafeRenego) {
    NS_ConvertASCIItoUTF16 msg(infoObject->GetHostName());
    msg.AppendLiteral(" : server does not support RFC 5746, see CVE-2009-3555");

    nsContentUtils::LogSimpleConsoleError(
        msg, "SSL", !!infoObject->GetOriginAttributes().mPrivateBrowsingId,
        true /* from chrome context */);
  }

  infoObject->NoteTimeUntilReady();
  infoObject->SetHandshakeCompleted();
}

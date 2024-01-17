/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCallbacks.h"

#include "NSSSocketControl.h"
#include "PSMRunnable.h"
#include "ScopedNSSTypes.h"
#include "SharedCertVerifier.h"
#include "SharedSSLState.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Span.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/intl/Localization.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIPrompt.h"
#include "nsIProtocolProxyService.h"
#include "nsISupportsPriority.h"
#include "nsIStreamLoader.h"
#include "nsIUploadChannel.h"
#include "nsIWebProgressListener.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSComponent.h"
#include "nsNSSHelper.h"
#include "nsNSSIOLayer.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsStringStream.h"
#include "mozpkix/pkixtypes.h"
#include "ssl.h"
#include "sslproto.h"
#include "SSLTokensCache.h"

#include "TrustOverrideUtils.h"
#include "TrustOverride-SymantecData.inc"
#include "TrustOverride-AppleGoogleDigiCertData.inc"

using namespace mozilla;
using namespace mozilla::pkix;
using namespace mozilla::psm;

extern LazyLogModule gPIPNSSLog;

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
  Monitor mMonitor MOZ_UNANNOTATED;
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
      mResponseResult(NS_ERROR_FAILURE) {
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

static constexpr auto OCSP_REQUEST_MIME_TYPE = "application/ocsp-request"_ns;
static constexpr auto OCSP_REQUEST_METHOD = "POST"_ns;

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
  nsresult rv = NS_NewURI(getter_AddRefs(uri), mAIALocation);
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
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
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

  channel->SetLoadFlags(
      nsIRequest::LOAD_ANONYMOUS | nsIRequest::LOAD_BYPASS_CACHE |
      nsIRequest::INHIBIT_CACHING | nsIChannel::LOAD_BYPASS_SERVICE_WORKER |
      nsIChannel::LOAD_BYPASS_URL_CLASSIFIER);

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();

  // Prevent HTTPS-Only Mode from upgrading the OCSP request.
  uint32_t httpsOnlyStatus = loadInfo->GetHttpsOnlyStatus();
  httpsOnlyStatus |= nsILoadInfo::HTTPS_ONLY_EXEMPT;
  loadInfo->SetHttpsOnlyStatus(httpsOnlyStatus);

  // allow deprecated HTTP request from SystemPrincipal
  loadInfo->SetAllowDeprecatedSystemRequests(true);

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
  // Do not use SPDY or HTTP3 for internal security operations. It could result
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
  rv = internalChannel->SetAllowHttp3(false);
  if (NS_FAILED(rv)) {
    return NotifyDone(rv, lock);
  }
  rv = internalChannel->SetIsOCSP(true);
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

static char* ShowProtectedAuthPrompt(PK11SlotInfo* slot, nsIPrompt* prompt) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(slot);
  MOZ_ASSERT(prompt);
  if (!NS_IsMainThread() || !slot || !prompt) {
    return nullptr;
  }

  // Dispatch a background task to (eventually) call C_Login. The call will
  // block until the protected authentication succeeds or fails.
  Atomic<bool> done;
  Atomic<SECStatus> result;
  nsresult rv =
      NS_DispatchBackgroundTask(NS_NewRunnableFunction(__func__, [&]() mutable {
        result = PK11_CheckUserPassword(slot, nullptr);
        done = true;
      }));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsTArray<nsCString> resIds = {
      "security/pippki/pippki.ftl"_ns,
  };
  RefPtr<mozilla::intl::Localization> l10n =
      mozilla::intl::Localization::Create(resIds, true);
  auto l10nId = "protected-auth-alert"_ns;
  auto l10nArgs = mozilla::dom::Optional<intl::L10nArgs>();
  l10nArgs.Construct();
  auto dirArg = l10nArgs.Value().Entries().AppendElement();
  dirArg->mKey = "tokenName"_ns;
  dirArg->mValue.SetValue().SetAsUTF8String().Assign(PK11_GetTokenName(slot));
  nsAutoCString promptString;
  ErrorResult errorResult;
  l10n->FormatValueSync(l10nId, l10nArgs, promptString, errorResult);
  if (NS_FAILED(errorResult.StealNSResult())) {
    return nullptr;
  }
  rv = prompt->Alert(nullptr, NS_ConvertUTF8toUTF16(promptString).get());
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil(
      "ShowProtectedAuthPrompt"_ns, [&]() { return static_cast<bool>(done); }));

  switch (result) {
    case SECSuccess:
      return ToNewCString(nsDependentCString(PK11_PW_AUTHENTICATED));
    case SECWouldBlock:
      return ToNewCString(nsDependentCString(PK11_PW_RETRY));
    default:
      return nullptr;
  }
}

class PK11PasswordPromptRunnable : public SyncRunnableBase {
 public:
  PK11PasswordPromptRunnable(PK11SlotInfo* slot, nsIInterfaceRequestor* ir)
      : mResult(nullptr), mSlot(slot), mIR(ir) {}
  virtual ~PK11PasswordPromptRunnable() = default;

  char* mResult;  // out
  virtual void RunOnTargetThread() override;

 private:
  static bool mRunning;

  PK11SlotInfo* mSlot;
  nsIInterfaceRequestor* mIR;
};

bool PK11PasswordPromptRunnable::mRunning = false;

void PK11PasswordPromptRunnable::RunOnTargetThread() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!NS_IsMainThread()) {
    return;
  }

  // If we've reentered due to the nested event loop implicit in using
  // nsIPrompt synchronously (or indeed the explicit nested event loop in the
  // protected authentication case), bail early, cancelling the password
  // prompt. This will probably cause the operation that resulted in the prompt
  // to fail, but this is better than littering the screen with a bunch of
  // password prompts that the user will probably just cancel anyway.
  if (mRunning) {
    return;
  }
  mRunning = true;
  auto setRunningToFalseOnExit = MakeScopeExit([&]() { mRunning = false; });

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
    mResult = ShowProtectedAuthPrompt(mSlot, prompt);
    return;
  }

  nsAutoString promptString;
  if (PK11_IsInternal(mSlot)) {
    rv = GetPIPNSSBundleString("CertPasswordPromptDefault", promptString);
  } else {
    AutoTArray<nsString, 1> formatStrings = {
        NS_ConvertUTF8toUTF16(PK11_GetTokenName(mSlot))};
    rv = PIPBundleFormatStringFromName("CertPasswordPrompt", formatStrings,
                                       promptString);
  }
  if (NS_FAILED(rv)) {
    return;
  }

  nsString password;
  bool userClickedOK = false;
  rv = prompt->PromptPassword(nullptr, promptString.get(),
                              getter_Copies(password), &userClickedOK);
  if (NS_FAILED(rv) || !userClickedOK) {
    return;
  }

  mResult = ToNewUTF8String(password);
}

char* PK11PasswordPrompt(PK11SlotInfo* slot, PRBool /*retry*/, void* arg) {
  if (!slot) {
    return nullptr;
  }
  RefPtr<PK11PasswordPromptRunnable> runnable(new PK11PasswordPromptRunnable(
      slot, static_cast<nsIInterfaceRequestor*>(arg)));
  runnable->DispatchToMainThreadAndWait();
  return runnable->mResult;
}

nsCString getKeaGroupName(uint32_t aKeaGroup) {
  nsCString groupName;
  switch (aKeaGroup) {
    case ssl_grp_ec_secp256r1:
      groupName = "P256"_ns;
      break;
    case ssl_grp_ec_secp384r1:
      groupName = "P384"_ns;
      break;
    case ssl_grp_ec_secp521r1:
      groupName = "P521"_ns;
      break;
    case ssl_grp_ec_curve25519:
      groupName = "x25519"_ns;
      break;
    case ssl_grp_kem_xyber768d00:
      groupName = "xyber768d00"_ns;
      break;
    case ssl_grp_ffdhe_2048:
      groupName = "FF 2048"_ns;
      break;
    case ssl_grp_ffdhe_3072:
      groupName = "FF 3072"_ns;
      break;
    case ssl_grp_none:
      groupName = "none"_ns;
      break;
    case ssl_grp_ffdhe_custom:
      groupName = "custom"_ns;
      break;
    // All other groups are not enabled in Firefox. See namedGroups in
    // nsNSSIOLayer.cpp.
    default:
      // This really shouldn't happen!
      MOZ_ASSERT_UNREACHABLE("Invalid key exchange group.");
      groupName = "unknown group"_ns;
  }
  return groupName;
}

nsCString getSignatureName(uint32_t aSignatureScheme) {
  nsCString signatureName;
  switch (aSignatureScheme) {
    case ssl_sig_none:
      signatureName = "none"_ns;
      break;
    case ssl_sig_rsa_pkcs1_sha1:
      signatureName = "RSA-PKCS1-SHA1"_ns;
      break;
    case ssl_sig_rsa_pkcs1_sha256:
      signatureName = "RSA-PKCS1-SHA256"_ns;
      break;
    case ssl_sig_rsa_pkcs1_sha384:
      signatureName = "RSA-PKCS1-SHA384"_ns;
      break;
    case ssl_sig_rsa_pkcs1_sha512:
      signatureName = "RSA-PKCS1-SHA512"_ns;
      break;
    case ssl_sig_ecdsa_secp256r1_sha256:
      signatureName = "ECDSA-P256-SHA256"_ns;
      break;
    case ssl_sig_ecdsa_secp384r1_sha384:
      signatureName = "ECDSA-P384-SHA384"_ns;
      break;
    case ssl_sig_ecdsa_secp521r1_sha512:
      signatureName = "ECDSA-P521-SHA512"_ns;
      break;
    case ssl_sig_rsa_pss_sha256:
      signatureName = "RSA-PSS-SHA256"_ns;
      break;
    case ssl_sig_rsa_pss_sha384:
      signatureName = "RSA-PSS-SHA384"_ns;
      break;
    case ssl_sig_rsa_pss_sha512:
      signatureName = "RSA-PSS-SHA512"_ns;
      break;
    case ssl_sig_ecdsa_sha1:
      signatureName = "ECDSA-SHA1"_ns;
      break;
    case ssl_sig_rsa_pkcs1_sha1md5:
      signatureName = "RSA-PKCS1-SHA1MD5"_ns;
      break;
    // All other groups are not enabled in Firefox. See sEnabledSignatureSchemes
    // in nsNSSIOLayer.cpp.
    default:
      // This really shouldn't happen!
      MOZ_ASSERT_UNREACHABLE("Invalid signature scheme.");
      signatureName = "unknown signature"_ns;
  }
  return signatureName;
}

static void PreliminaryHandshakeDone(PRFileDesc* fd) {
  NSSSocketControl* socketControl = (NSSSocketControl*)fd->higher->secret;
  if (!socketControl) {
    return;
  }
  if (socketControl->IsPreliminaryHandshakeDone()) {
    return;
  }

  SSLChannelInfo channelInfo;
  if (SSL_GetChannelInfo(fd, &channelInfo, sizeof(channelInfo)) != SECSuccess) {
    return;
  }
  SSLCipherSuiteInfo cipherInfo;
  if (SSL_GetCipherSuiteInfo(channelInfo.cipherSuite, &cipherInfo,
                             sizeof(cipherInfo)) != SECSuccess) {
    return;
  }
  socketControl->SetPreliminaryHandshakeInfo(channelInfo, cipherInfo);
  socketControl->SetSSLVersionUsed(channelInfo.protocolVersion);
  socketControl->SetEarlyDataAccepted(channelInfo.earlyDataAccepted);
  socketControl->SetKEAUsed(channelInfo.keaType);
  socketControl->SetKEAKeyBits(channelInfo.keaKeyBits);
  socketControl->SetMACAlgorithmUsed(cipherInfo.macAlgorithm);

  // Get the NPN value.
  SSLNextProtoState state;
  unsigned char npnbuf[256];
  unsigned int npnlen;

  if (SSL_GetNextProto(fd, &state, npnbuf, &npnlen,
                       AssertedCast<unsigned int>(ArrayLength(npnbuf))) ==
      SECSuccess) {
    if (state == SSL_NEXT_PROTO_NEGOTIATED ||
        state == SSL_NEXT_PROTO_SELECTED) {
      socketControl->SetNegotiatedNPN(
          BitwiseCast<char*, unsigned char*>(npnbuf), npnlen);
    } else {
      socketControl->SetNegotiatedNPN(nullptr, 0);
    }
    mozilla::Telemetry::Accumulate(Telemetry::SSL_NPN_TYPE, state);
  } else {
    socketControl->SetNegotiatedNPN(nullptr, 0);
  }

  socketControl->SetPreliminaryHandshakeDone();
}

SECStatus CanFalseStartCallback(PRFileDesc* fd, void* client_data,
                                PRBool* canFalseStart) {
  *canFalseStart = false;

  NSSSocketControl* infoObject = (NSSSocketControl*)fd->higher->secret;
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
  // Also note that ecdh_hybrid groups are not supported in TLS 1.2 and are out
  // of scope.
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
  unsigned int value = bits < 512      ? 1
                       : bits == 512   ? 2
                       : bits < 768    ? 3
                       : bits == 768   ? 4
                       : bits < 1024   ? 5
                       : bits == 1024  ? 6
                       : bits < 1280   ? 7
                       : bits == 1280  ? 8
                       : bits < 1536   ? 9
                       : bits == 1536  ? 10
                       : bits < 2048   ? 11
                       : bits == 2048  ? 12
                       : bits < 3072   ? 13
                       : bits == 3072  ? 14
                       : bits < 4096   ? 15
                       : bits == 4096  ? 16
                       : bits < 8192   ? 17
                       : bits == 8192  ? 18
                       : bits < 16384  ? 19
                       : bits == 16384 ? 20
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
  unsigned int value = bits == 255   ? 29  // Curve25519
                       : bits == 256 ? 23  // P-256
                       : bits == 384 ? 24  // P-384
                       : bits == 521 ? 25  // P-521
                                     : 0;  // Unknown
  Telemetry::Accumulate(probe, value);
}

static void AccumulateCipherSuite(const SSLChannelInfo& channelInfo) {
  uint32_t value;
  // Note: this list must include every cipher suite it is possible to enable
  // in nsNSSComponent.cpp (see sCipherPrefs and sDeprecatedTLS1CipherPrefs).
  switch (channelInfo.cipherSuite) {
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:  // 0x000A
      value = 1;
      break;
    case TLS_RSA_WITH_AES_128_CBC_SHA:  // 0x002F
      value = 2;
      break;
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:  // 0x0033
      value = 3;
      break;
    case TLS_RSA_WITH_AES_256_CBC_SHA:  // 0x0035
      value = 4;
      break;
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:  // 0x0039
      value = 5;
      break;
    case TLS_RSA_WITH_AES_128_GCM_SHA256:  // 0x009C
      value = 6;
      break;
    case TLS_RSA_WITH_AES_256_GCM_SHA384:  // 0x009D
      value = 7;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:  // 0xC009
      value = 8;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:  // 0xC00A
      value = 9;
      break;
    case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:  // 0xC013
      value = 10;
      break;
    case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:  // 0xC014
      value = 11;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:  // 0xC02B
      value = 12;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:  // 0xC02C
      value = 13;
      break;
    case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:  // 0xC02F
      value = 14;
      break;
    case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:  // 0xC030
      value = 15;
      break;
    case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:  // 0xCCA8
      value = 16;
      break;
    case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:  // 0xCCA9
      value = 17;
      break;

    // TLS 1.3 cipher suites
    case TLS_AES_128_GCM_SHA256:  // 0x1301
      value = 18;
      break;
    case TLS_AES_256_GCM_SHA384:  // 0x1302
      value = 19;
      break;
    case TLS_CHACHA20_POLY1305_SHA256:  // 0x1303
      value = 20;
      break;

    // unknown
    default:
      value = 0;
      break;
  }
  MOZ_ASSERT(value != 0);
  Telemetry::Accumulate(Telemetry::TLS_CIPHER_SUITE, value);
}

void HandshakeCallback(PRFileDesc* fd, void* client_data) {
  // Do the bookkeeping that needs to be done after the
  // server's ServerHello...ServerHelloDone have been processed, but that
  // doesn't need the handshake to be completed.
  PreliminaryHandshakeDone(fd);

  NSSSocketControl* infoObject = (NSSSocketControl*)fd->higher->secret;
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
  SECStatus rv = SSL_GetChannelInfo(fd, &channelInfo, sizeof(channelInfo));
  MOZ_ASSERT(rv == SECSuccess);
  if (rv != SECSuccess) {
    return;
  }
  AccumulateCipherSuite(channelInfo);

  // Get the protocol version for telemetry
  // 1=tls1, 2=tls1.1, 3=tls1.2, 4=tls1.3
  unsigned int versionEnum = channelInfo.protocolVersion & 0xFF;
  MOZ_ASSERT(versionEnum > 0);
  Telemetry::Accumulate(Telemetry::SSL_HANDSHAKE_VERSION, versionEnum);

  SSLCipherSuiteInfo cipherInfo;
  rv = SSL_GetCipherSuiteInfo(channelInfo.cipherSuite, &cipherInfo,
                              sizeof cipherInfo);
  MOZ_ASSERT(rv == SECSuccess);
  if (rv != SECSuccess) {
    return;
  }
  // keyExchange null=0, rsa=1, dh=2, fortezza=3, ecdh=4, ecdh_hybrid=5
  Telemetry::Accumulate(infoObject->IsFullHandshake()
                            ? Telemetry::SSL_KEY_EXCHANGE_ALGORITHM_FULL
                            : Telemetry::SSL_KEY_EXCHANGE_ALGORITHM_RESUMED,
                        channelInfo.keaType);

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
      case ssl_kea_ecdh_hybrid:
        // Bug 1874963: Add probes for Xyber768d00
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
    infoObject->RebuildCertificateInfoFromSSLTokenCache();
  }

  // Check if the user has added an override for a certificate error.
  if (infoObject->HasUserOverriddenCertificateError()) {
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
        msg, "SSL"_ns, !!infoObject->GetOriginAttributes().mPrivateBrowsingId,
        true /* from chrome context */);
  }

  infoObject->NoteTimeUntilReady();
  infoObject->SetHandshakeCompleted();
}

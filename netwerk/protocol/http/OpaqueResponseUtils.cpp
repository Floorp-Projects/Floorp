/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/OpaqueResponseUtils.h"

#include "mozilla/dom/Document.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/dom/JSValidatorParent.h"
#include "ErrorList.h"
#include "nsContentUtils.h"
#include "nsHttpResponseHead.h"
#include "nsISupports.h"
#include "nsMimeTypes.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsStringStream.h"
#include "HttpBaseChannel.h"

static mozilla::LazyLogModule gORBLog("ORB");

#define LOGORB(msg, ...)            \
  MOZ_LOG(gORBLog, LogLevel::Debug, \
          ("%s: %p " msg, __func__, this, ##__VA_ARGS__))

namespace mozilla::net {

static bool IsOpaqueSafeListedMIMEType(const nsACString& aContentType) {
  if (aContentType.EqualsLiteral(TEXT_CSS) ||
      aContentType.EqualsLiteral(IMAGE_SVG_XML)) {
    return true;
  }

  NS_ConvertUTF8toUTF16 typeString(aContentType);
  return nsContentUtils::IsJavascriptMIMEType(typeString);
}

// These need to be kept in sync with
// "browser.opaqueResponseBlocking.mediaExceptionsStrategy"
enum class OpaqueResponseMediaException { NoExceptions, AllowSome, AllowAll };

static OpaqueResponseMediaException ConfiguredMediaExceptionsStrategy() {
  uint32_t pref = StaticPrefs::
      browser_opaqueResponseBlocking_mediaExceptionsStrategy_DoNotUseDirectly();
  if (NS_WARN_IF(pref > static_cast<uint32_t>(
                            OpaqueResponseMediaException::AllowAll))) {
    return OpaqueResponseMediaException::AllowAll;
  }

  return static_cast<OpaqueResponseMediaException>(pref);
}

static bool IsOpaqueSafeListedSpecBreakingMIMEType(
    const nsACString& aContentType, bool aNoSniff) {
  // Avoid trouble with DASH/HLS. See bug 1698040.
  if (aContentType.EqualsLiteral(APPLICATION_DASH_XML) ||
      aContentType.EqualsLiteral(APPLICATION_MPEGURL) ||
      aContentType.EqualsLiteral(AUDIO_MPEG_URL) ||
      aContentType.EqualsLiteral(TEXT_VTT)) {
    return true;
  }

  // Do what Chromium does. This is from bug 1828375, and we should ideally
  // revert this.
  if (aContentType.EqualsLiteral(TEXT_PLAIN) && aNoSniff) {
    return true;
  }

  switch (ConfiguredMediaExceptionsStrategy()) {
    case OpaqueResponseMediaException::NoExceptions:
      break;
    case OpaqueResponseMediaException::AllowSome:
      if (aContentType.EqualsLiteral(AUDIO_MP3) ||
          aContentType.EqualsLiteral(AUDIO_AAC) ||
          aContentType.EqualsLiteral(AUDIO_AACP) ||
          aContentType.EqualsLiteral(MULTIPART_MIXED_REPLACE)) {
        return true;
      }
      break;
    case OpaqueResponseMediaException::AllowAll:
      if (StringBeginsWith(aContentType, "audio/"_ns) ||
          StringBeginsWith(aContentType, "video/"_ns) ||
          aContentType.EqualsLiteral(MULTIPART_MIXED_REPLACE)) {
        return true;
      }
      break;
  }

  return false;
}

static bool IsOpaqueBlockListedMIMEType(const nsACString& aContentType) {
  return aContentType.EqualsLiteral(TEXT_HTML) ||
         StringEndsWith(aContentType, "+json"_ns) ||
         aContentType.EqualsLiteral(APPLICATION_JSON) ||
         aContentType.EqualsLiteral(TEXT_JSON) ||
         StringEndsWith(aContentType, "+xml"_ns) ||
         aContentType.EqualsLiteral(APPLICATION_XML) ||
         aContentType.EqualsLiteral(TEXT_XML);
}

static bool IsOpaqueBlockListedNeverSniffedMIMEType(
    const nsACString& aContentType) {
  return aContentType.EqualsLiteral(APPLICATION_GZIP2) ||
         aContentType.EqualsLiteral(APPLICATION_MSEXCEL) ||
         aContentType.EqualsLiteral(APPLICATION_MSPPT) ||
         aContentType.EqualsLiteral(APPLICATION_MSWORD) ||
         aContentType.EqualsLiteral(APPLICATION_MSWORD_TEMPLATE) ||
         aContentType.EqualsLiteral(APPLICATION_PDF) ||
         aContentType.EqualsLiteral(APPLICATION_MPEGURL) ||
         aContentType.EqualsLiteral(APPLICATION_VND_CES_QUICKPOINT) ||
         aContentType.EqualsLiteral(APPLICATION_VND_CES_QUICKSHEET) ||
         aContentType.EqualsLiteral(APPLICATION_VND_CES_QUICKWORD) ||
         aContentType.EqualsLiteral(APPLICATION_VND_MS_EXCEL) ||
         aContentType.EqualsLiteral(APPLICATION_VND_MS_EXCEL2) ||
         aContentType.EqualsLiteral(APPLICATION_VND_MS_PPT) ||
         aContentType.EqualsLiteral(APPLICATION_VND_MS_PPT2) ||
         aContentType.EqualsLiteral(APPLICATION_VND_MS_WORD) ||
         aContentType.EqualsLiteral(APPLICATION_VND_MS_WORD2) ||
         aContentType.EqualsLiteral(APPLICATION_VND_MS_WORD3) ||
         aContentType.EqualsLiteral(APPLICATION_VND_MSWORD) ||
         aContentType.EqualsLiteral(
             APPLICATION_VND_PRESENTATIONML_PRESENTATION) ||
         aContentType.EqualsLiteral(APPLICATION_VND_PRESENTATIONML_TEMPLATE) ||
         aContentType.EqualsLiteral(APPLICATION_VND_SPREADSHEETML_SHEET) ||
         aContentType.EqualsLiteral(APPLICATION_VND_SPREADSHEETML_TEMPLATE) ||
         aContentType.EqualsLiteral(
             APPLICATION_VND_WORDPROCESSINGML_DOCUMENT) ||
         aContentType.EqualsLiteral(
             APPLICATION_VND_WORDPROCESSINGML_TEMPLATE) ||
         aContentType.EqualsLiteral(APPLICATION_VND_PRESENTATION_OPENXML) ||
         aContentType.EqualsLiteral(APPLICATION_VND_PRESENTATION_OPENXMLM) ||
         aContentType.EqualsLiteral(APPLICATION_VND_SPREADSHEET_OPENXML) ||
         aContentType.EqualsLiteral(APPLICATION_VND_WORDPROSSING_OPENXML) ||
         aContentType.EqualsLiteral(APPLICATION_GZIP) ||
         aContentType.EqualsLiteral(APPLICATION_XPROTOBUF) ||
         aContentType.EqualsLiteral(APPLICATION_XPROTOBUFFER) ||
         aContentType.EqualsLiteral(APPLICATION_ZIP) ||
         aContentType.EqualsLiteral(AUDIO_MPEG_URL) ||
         aContentType.EqualsLiteral(MULTIPART_BYTERANGES) ||
         aContentType.EqualsLiteral(MULTIPART_SIGNED) ||
         aContentType.EqualsLiteral(TEXT_EVENT_STREAM) ||
         aContentType.EqualsLiteral(TEXT_CSV) ||
         aContentType.EqualsLiteral(TEXT_VTT) ||
         aContentType.EqualsLiteral(APPLICATION_DASH_XML);
}

OpaqueResponseBlockedReason GetOpaqueResponseBlockedReason(
    const nsACString& aContentType, uint16_t aStatus, bool aNoSniff) {
  if (aContentType.IsEmpty()) {
    return OpaqueResponseBlockedReason::BLOCKED_SHOULD_SNIFF;
  }

  if (IsOpaqueSafeListedMIMEType(aContentType)) {
    return OpaqueResponseBlockedReason::ALLOWED_SAFE_LISTED;
  }

  // For some MIME types we deviate from spec and allow when we ideally
  // shouldn't. These are returnened before any blocking takes place.
  if (IsOpaqueSafeListedSpecBreakingMIMEType(aContentType, aNoSniff)) {
    return OpaqueResponseBlockedReason::ALLOWED_SAFE_LISTED_SPEC_BREAKING;
  }

  if (IsOpaqueBlockListedNeverSniffedMIMEType(aContentType)) {
    return OpaqueResponseBlockedReason::BLOCKED_BLOCKLISTED_NEVER_SNIFFED;
  }

  if (aStatus == 206 && IsOpaqueBlockListedMIMEType(aContentType)) {
    return OpaqueResponseBlockedReason::BLOCKED_206_AND_BLOCKLISTED;
  }

  nsAutoCString contentTypeOptionsHeader;
  if (aNoSniff && (IsOpaqueBlockListedMIMEType(aContentType) ||
                   aContentType.EqualsLiteral(TEXT_PLAIN))) {
    return OpaqueResponseBlockedReason::
        BLOCKED_NOSNIFF_AND_EITHER_BLOCKLISTED_OR_TEXTPLAIN;
  }

  return OpaqueResponseBlockedReason::BLOCKED_SHOULD_SNIFF;
}

OpaqueResponseBlockedReason GetOpaqueResponseBlockedReason(
    nsHttpResponseHead& aResponseHead) {
  nsAutoCString contentType;
  aResponseHead.ContentType(contentType);

  nsAutoCString contentTypeOptionsHeader;
  bool nosniff =
      aResponseHead.GetContentTypeOptionsHeader(contentTypeOptionsHeader) &&
      contentTypeOptionsHeader.EqualsIgnoreCase("nosniff");

  return GetOpaqueResponseBlockedReason(contentType, aResponseHead.Status(),
                                        nosniff);
}

Result<std::tuple<int64_t, int64_t, int64_t>, nsresult>
ParseContentRangeHeaderString(const nsAutoCString& aRangeStr) {
  // Parse the range header: e.g. Content-Range: bytes 7000-7999/8000.
  const int32_t spacePos = aRangeStr.Find(" "_ns);
  const int32_t dashPos = aRangeStr.Find("-"_ns, spacePos);
  const int32_t slashPos = aRangeStr.Find("/"_ns, dashPos);

  nsAutoCString rangeStartText;
  aRangeStr.Mid(rangeStartText, spacePos + 1, dashPos - (spacePos + 1));

  nsresult rv;
  const int64_t rangeStart = rangeStartText.ToInteger64(&rv);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  if (0 > rangeStart) {
    return Err(NS_ERROR_ILLEGAL_VALUE);
  }

  nsAutoCString rangeEndText;
  aRangeStr.Mid(rangeEndText, dashPos + 1, slashPos - (dashPos + 1));
  const int64_t rangeEnd = rangeEndText.ToInteger64(&rv);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  if (rangeStart > rangeEnd) {
    return Err(NS_ERROR_ILLEGAL_VALUE);
  }

  nsAutoCString rangeTotalText;
  aRangeStr.Right(rangeTotalText, aRangeStr.Length() - (slashPos + 1));
  if (rangeTotalText[0] == '*') {
    return std::make_tuple(rangeStart, rangeEnd, (int64_t)-1);
  }

  const int64_t rangeTotal = rangeTotalText.ToInteger64(&rv);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  if (rangeEnd >= rangeTotal) {
    return Err(NS_ERROR_ILLEGAL_VALUE);
  }

  return std::make_tuple(rangeStart, rangeEnd, rangeTotal);
}

bool IsFirstPartialResponse(nsHttpResponseHead& aResponseHead) {
  MOZ_ASSERT(aResponseHead.Status() == 206);

  nsAutoCString contentRange;
  Unused << aResponseHead.GetHeader(nsHttp::Content_Range, contentRange);

  auto rangeOrErr = ParseContentRangeHeaderString(contentRange);
  if (rangeOrErr.isErr()) {
    return false;
  }

  const int64_t responseFirstBytePos = std::get<0>(rangeOrErr.unwrap());
  return responseFirstBytePos == 0;
}

LogModule* GetORBLog() { return gORBLog; }

OpaqueResponseFilter::OpaqueResponseFilter(nsIStreamListener* aNext)
    : mNext(aNext) {
  LOGORB();
}

NS_IMETHODIMP
OpaqueResponseFilter::OnStartRequest(nsIRequest* aRequest) {
  LOGORB();
  nsCOMPtr<HttpBaseChannel> httpBaseChannel = do_QueryInterface(aRequest);
  MOZ_ASSERT(httpBaseChannel);

  nsHttpResponseHead* responseHead = httpBaseChannel->GetResponseHead();

  if (responseHead) {
    // Filtered opaque responses doesn't need headers, so we just drop them.
    responseHead->ClearHeaders();
  }

  mNext->OnStartRequest(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
OpaqueResponseFilter::OnDataAvailable(nsIRequest* aRequest,
                                      nsIInputStream* aInputStream,
                                      uint64_t aOffset, uint32_t aCount) {
  LOGORB();
  uint32_t result;
  // No data for filtered opaque responses should reach the content process, so
  // we just discard them.
  return aInputStream->ReadSegments(NS_DiscardSegment, nullptr, aCount,
                                    &result);
}

NS_IMETHODIMP
OpaqueResponseFilter::OnStopRequest(nsIRequest* aRequest,
                                    nsresult aStatusCode) {
  LOGORB();
  mNext->OnStopRequest(aRequest, aStatusCode);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(OpaqueResponseFilter, nsIStreamListener, nsIRequestObserver)

OpaqueResponseBlocker::OpaqueResponseBlocker(nsIStreamListener* aNext,
                                             HttpBaseChannel* aChannel,
                                             const nsCString& aContentType,
                                             bool aNoSniff)
    : mNext(aNext), mContentType(aContentType), mNoSniff(aNoSniff) {
  // Storing aChannel as a member is tricky as aChannel owns us and it's
  // hard to ensure aChannel is alive when we about to use it without
  // creating a cycle. This is all doable but need some extra efforts.
  //
  // So we are just passing aChannel from the caller when we need to use it.
  MOZ_ASSERT(aChannel);

  if (MOZ_UNLIKELY(MOZ_LOG_TEST(gORBLog, LogLevel::Debug))) {
    nsCOMPtr<nsIURI> uri;
    aChannel->GetURI(getter_AddRefs(uri));
    if (uri) {
      LOGORB(" channel=%p, uri=%s", aChannel, uri->GetSpecOrDefault().get());
    }
  }
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  MOZ_DIAGNOSTIC_ASSERT(aChannel->CachedOpaqueResponseBlockingPref());
}

NS_IMETHODIMP
OpaqueResponseBlocker::OnStartRequest(nsIRequest* aRequest) {
  LOGORB();

  if (mState == State::Sniffing) {
    Unused << EnsureOpaqueResponseIsAllowedAfterSniff(aRequest);
  }

  // mState will remain State::Sniffing if we need to wait
  // for JS validator to make a decision.
  //
  // When the state is Sniffing, we can't call mNext->OnStartRequest
  // because fetch requests need the cancellation to be done
  // before its FetchDriver::OnStartRequest is called, otherwise it'll
  // resolve the promise regardless the decision of JS validator.
  if (mState != State::Sniffing) {
    nsresult rv = mNext->OnStartRequest(aRequest);
    return NS_SUCCEEDED(mStatus) ? rv : mStatus;
  }

  return NS_OK;
}

NS_IMETHODIMP
OpaqueResponseBlocker::OnStopRequest(nsIRequest* aRequest,
                                     nsresult aStatusCode) {
  LOGORB();

  nsresult statusForStop = aStatusCode;

  if (mState == State::Blocked && NS_FAILED(mStatus)) {
    statusForStop = mStatus;
  }

  if (mState == State::Sniffing) {
    // It is the call to JSValidatorParent::OnStopRequest that will trigger the
    // JS parser.
    mStartOfJavaScriptValidation = TimeStamp::Now();

    MOZ_ASSERT(mJSValidator);
    mPendingOnStopRequestStatus = Some(aStatusCode);
    mJSValidator->OnStopRequest(aStatusCode, *aRequest);
    return NS_OK;
  }

  return mNext->OnStopRequest(aRequest, statusForStop);
}

NS_IMETHODIMP
OpaqueResponseBlocker::OnDataAvailable(nsIRequest* aRequest,
                                       nsIInputStream* aInputStream,
                                       uint64_t aOffset, uint32_t aCount) {
  LOGORB();

  if (mState == State::Allowed) {
    return mNext->OnDataAvailable(aRequest, aInputStream, aOffset, aCount);
  }

  if (mState == State::Blocked) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(mState == State::Sniffing);

  nsCString data;
  if (!data.SetLength(aCount, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t read;
  nsresult rv = aInputStream->Read(data.BeginWriting(), aCount, &read);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(mJSValidator);

  mJSValidator->OnDataAvailable(data);

  return NS_OK;
}

nsresult OpaqueResponseBlocker::EnsureOpaqueResponseIsAllowedAfterSniff(
    nsIRequest* aRequest) {
  nsCOMPtr<HttpBaseChannel> httpBaseChannel = do_QueryInterface(aRequest);
  MOZ_ASSERT(httpBaseChannel);

  // The `AfterSniff` check shouldn't be run when
  // 1. We have made a decision already
  // 2. The JS validator is running, so we should wait
  // for its result.
  if (mState != State::Sniffing || mJSValidator) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> loadInfo;

  nsresult rv =
      httpBaseChannel->GetLoadInfo(getter_AddRefs<nsILoadInfo>(loadInfo));
  if (NS_FAILED(rv)) {
    LOGORB("Failed to get LoadInfo");
    BlockResponse(httpBaseChannel, rv);
    return rv;
  }

  nsCOMPtr<nsIURI> uri;
  rv = httpBaseChannel->GetURI(getter_AddRefs<nsIURI>(uri));
  if (NS_FAILED(rv)) {
    LOGORB("Failed to get uri");
    BlockResponse(httpBaseChannel, rv);
    return rv;
  }

  switch (httpBaseChannel->PerformOpaqueResponseSafelistCheckAfterSniff(
      mContentType, mNoSniff)) {
    case OpaqueResponse::Block:
      BlockResponse(httpBaseChannel, NS_ERROR_FAILURE);
      return NS_ERROR_FAILURE;
    case OpaqueResponse::Allow:
      AllowResponse();
      return NS_OK;
    case OpaqueResponse::Sniff:
    case OpaqueResponse::SniffCompressed:
      break;
  }

  MOZ_ASSERT(mState == State::Sniffing);
  return ValidateJavaScript(httpBaseChannel, uri, loadInfo);
}

OpaqueResponse
OpaqueResponseBlocker::EnsureOpaqueResponseIsAllowedAfterJavaScriptValidation(
    HttpBaseChannel* aChannel, bool aAllow) {
  if (aAllow) {
    return OpaqueResponse::Allow;
  }

  return aChannel->BlockOrFilterOpaqueResponse(
      this, u"Javascript validation failed"_ns,
      OpaqueResponseBlockedTelemetryReason::JS_VALIDATION_FAILED,
      "Javascript validation failed");
}

static void RecordTelemetry(const TimeStamp& aStartOfValidation,
                            const TimeStamp& aStartOfJavaScriptValidation,
                            OpaqueResponseBlocker::ValidatorResult aResult) {
  using ValidatorResult = OpaqueResponseBlocker::ValidatorResult;
  MOZ_DIAGNOSTIC_ASSERT(aStartOfValidation);

  auto key = [aResult]() {
    switch (aResult) {
      case ValidatorResult::JavaScript:
        return "javascript"_ns;
      case ValidatorResult::JSON:
        return "json"_ns;
      case ValidatorResult::Other:
        return "other"_ns;
      case ValidatorResult::Failure:
        return "failure"_ns;
    }
    MOZ_ASSERT_UNREACHABLE("Switch statement should be saturated");
    return "failure"_ns;
  }();

  TimeStamp now = TimeStamp::Now();
  PROFILER_MARKER_TEXT(
      "ORB safelist check", NETWORK,
      MarkerTiming::Interval(aStartOfValidation, aStartOfJavaScriptValidation),
      nsPrintfCString("Receive data for validation (%s)", key.get()));

  PROFILER_MARKER_TEXT(
      "ORB safelist check", NETWORK,
      MarkerTiming::Interval(aStartOfJavaScriptValidation, now),
      nsPrintfCString("JS Validation (%s)", key.get()));

  Telemetry::AccumulateTimeDelta(Telemetry::ORB_RECEIVE_DATA_FOR_VALIDATION_MS,
                                 key, aStartOfValidation,
                                 aStartOfJavaScriptValidation);

  Telemetry::AccumulateTimeDelta(Telemetry::ORB_JAVASCRIPT_VALIDATION_MS, key,
                                 aStartOfJavaScriptValidation, now);
}

// The specification for ORB is currently being written:
// https://whatpr.org/fetch/1442.html#orb-algorithm
// The `opaque-response-safelist check` is implemented in:
// * `HttpBaseChannel::OpaqueResponseSafelistCheckBeforeSniff`
// * `nsHttpChannel::DisableIsOpaqueResponseAllowedAfterSniffCheck`
// * `HttpBaseChannel::OpaqueResponseSafelistCheckAfterSniff`
// * `OpaqueResponseBlocker::ValidateJavaScript`
nsresult OpaqueResponseBlocker::ValidateJavaScript(HttpBaseChannel* aChannel,
                                                   nsIURI* aURI,
                                                   nsILoadInfo* aLoadInfo) {
  MOZ_DIAGNOSTIC_ASSERT(aChannel);
  MOZ_ASSERT(aURI && aLoadInfo);

  if (!StaticPrefs::browser_opaqueResponseBlocking_javascriptValidator()) {
    LOGORB("Allowed: JS Validator is disabled");
    AllowResponse();
    return NS_OK;
  }

  int64_t contentLength;
  nsresult rv = aChannel->GetContentLength(&contentLength);
  if (NS_FAILED(rv)) {
    LOGORB("Blocked: No Content Length");
    BlockResponse(aChannel, rv);
    return rv;
  }

  Telemetry::ScalarAdd(
      Telemetry::ScalarID::OPAQUE_RESPONSE_BLOCKING_JAVASCRIPT_VALIDATION_COUNT,
      1);

  LOGORB("Send %s to the validator", aURI->GetSpecOrDefault().get());
  // https://whatpr.org/fetch/1442.html#orb-algorithm, step 15
  mJSValidator = dom::JSValidatorParent::Create();
  mJSValidator->IsOpaqueResponseAllowed(
      [self = RefPtr{this}, channel = nsCOMPtr{aChannel}, uri = nsCOMPtr{aURI},
       loadInfo = nsCOMPtr{aLoadInfo}, startOfValidation = TimeStamp::Now()](
          Maybe<ipc::Shmem> aSharedData, ValidatorResult aResult) {
        MOZ_LOG(gORBLog, LogLevel::Debug,
                ("JSValidator resolved for %s with %s",
                 uri->GetSpecOrDefault().get(),
                 aSharedData.isSome() ? "true" : "false"));
        bool allowed = aResult == ValidatorResult::JavaScript;
        switch (self->EnsureOpaqueResponseIsAllowedAfterJavaScriptValidation(
            channel, allowed)) {
          case OpaqueResponse::Allow:
            // It's possible that the JS validation failed for this request,
            // however we decided that we need to filter the response instead
            // of blocking. So we set allowed to true manually when that's the
            // case.
            allowed = true;
            self->AllowResponse();
            break;
          case OpaqueResponse::Block:
            // We'll filter the data out later
            self->AllowResponse();
            break;
          default:
            MOZ_ASSERT_UNREACHABLE(
                "We should only ever have Allow or Block here.");
            allowed = false;
            self->BlockResponse(channel, NS_ERROR_FAILURE);
            break;
        }

        self->ResolveAndProcessData(channel, allowed, aSharedData);
        if (aSharedData.isSome()) {
          self->mJSValidator->DeallocShmem(aSharedData.ref());
        }

        RecordTelemetry(startOfValidation, self->mStartOfJavaScriptValidation,
                        aResult);

        Unused << dom::PJSValidatorParent::Send__delete__(self->mJSValidator);
        self->mJSValidator = nullptr;
      });

  return NS_OK;
}

bool OpaqueResponseBlocker::IsSniffing() const {
  return mState == State::Sniffing;
}

void OpaqueResponseBlocker::AllowResponse() {
  LOGORB("Sniffer is done, allow response, this=%p", this);
  MOZ_ASSERT(mState == State::Sniffing);
  mState = State::Allowed;
}

void OpaqueResponseBlocker::BlockResponse(HttpBaseChannel* aChannel,
                                          nsresult aStatus) {
  LOGORB("Sniffer is done, block response, this=%p", this);
  MOZ_ASSERT(mState == State::Sniffing);
  mState = State::Blocked;
  mStatus = aStatus;
  aChannel->SetChannelBlockedByOpaqueResponse();
  aChannel->CancelWithReason(mStatus,
                             "OpaqueResponseBlocker::BlockResponse"_ns);
}

void OpaqueResponseBlocker::FilterResponse() {
  MOZ_ASSERT(mState == State::Sniffing);

  if (mShouldFilter) {
    return;
  }

  mShouldFilter = true;

  mNext = new OpaqueResponseFilter(mNext);
}

void OpaqueResponseBlocker::ResolveAndProcessData(
    HttpBaseChannel* aChannel, bool aAllowed, Maybe<ipc::Shmem>& aSharedData) {
  if (!aAllowed) {
    // OpaqueResponseFilter allows us to filter the headers
    mNext = new OpaqueResponseFilter(mNext);
  }

  nsresult rv = OnStartRequest(aChannel);

  if (!aAllowed || NS_FAILED(rv)) {
    MOZ_ASSERT_IF(!aAllowed, mState == State::Allowed);
    // No need to call OnDataAvailable because
    //   1. The input stream is consumed by
    //     OpaqueResponseBlocker::OnDataAvailable already
    //   2. We don't want to pass any data over
    MaybeRunOnStopRequest(aChannel);
    return;
  }

  MOZ_ASSERT(mState == State::Allowed);

  if (aSharedData.isNothing()) {
    MaybeRunOnStopRequest(aChannel);
    return;
  }

  const ipc::Shmem& mem = aSharedData.ref();
  nsCOMPtr<nsIInputStream> input;
  rv = NS_NewByteInputStream(getter_AddRefs(input),
                             Span(mem.get<char>(), mem.Size<char>()),
                             NS_ASSIGNMENT_DEPEND);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    BlockResponse(aChannel, rv);
    MaybeRunOnStopRequest(aChannel);
    return;
  }

  // When this line reaches, the state is either State::Allowed or
  // State::Blocked. The OnDataAvailable call will either call
  // the next listener or reject the request.
  OnDataAvailable(aChannel, input, 0, mem.Size<char>());

  MaybeRunOnStopRequest(aChannel);
}

void OpaqueResponseBlocker::MaybeRunOnStopRequest(HttpBaseChannel* aChannel) {
  MOZ_ASSERT(mState != State::Sniffing);
  if (mPendingOnStopRequestStatus.isSome()) {
    OnStopRequest(aChannel, mPendingOnStopRequestStatus.value());
  }
}

NS_IMPL_ISUPPORTS(OpaqueResponseBlocker, nsIStreamListener, nsIRequestObserver)

}  // namespace mozilla::net

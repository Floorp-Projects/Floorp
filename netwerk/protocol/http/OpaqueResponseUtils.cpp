/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/OpaqueResponseUtils.h"

#include "mozilla/dom/Document.h"
#include "ErrorList.h"
#include "nsContentUtils.h"
#include "nsHttpResponseHead.h"
#include "nsISupports.h"
#include "nsMimeTypes.h"
#include "nsThreadUtils.h"
#include "nsStringStream.h"
#include "HttpBaseChannel.h"

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
         aContentType.EqualsLiteral(TEXT_VTT);
}

OpaqueResponseBlockedReason GetOpaqueResponseBlockedReason(
    const nsACString& aContentType, uint16_t aStatus, bool aNoSniff) {
  if (aContentType.IsEmpty()) {
    return OpaqueResponseBlockedReason::BLOCKED_SHOULD_SNIFF;
  }

  if (IsOpaqueSafeListedMIMEType(aContentType)) {
    return OpaqueResponseBlockedReason::ALLOWED_SAFE_LISTED;
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
    const nsHttpResponseHead& aResponseHead) {
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

void LogORBError(nsILoadInfo* aLoadInfo, nsIURI* aURI) {
  RefPtr<dom::Document> doc;
  aLoadInfo->GetLoadingDocument(getter_AddRefs(doc));

  nsAutoCString uri;
  nsresult rv = nsContentUtils::AnonymizeURI(aURI, uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  AutoTArray<nsString, 1> params;
  CopyUTF8toUTF16(uri, *params.AppendElement());

  MOZ_LOG(gORBLog, LogLevel::Debug,
          ("%s: Resource blocked: %s ", __func__, uri.get()));
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "ORB"_ns, doc,
                                  nsContentUtils::eNECKO_PROPERTIES,
                                  "ResourceBlockedCORS", params);
}

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

  Unused << EnsureOpaqueResponseIsAllowedAfterSniff(aRequest);

  MOZ_ASSERT(mState != State::Sniffing);

  nsresult rv = mNext->OnStartRequest(aRequest);
  return NS_SUCCEEDED(mStatus) ? rv : mStatus;
}

NS_IMETHODIMP
OpaqueResponseBlocker::OnStopRequest(nsIRequest* aRequest,
                                     nsresult aStatusCode) {
  LOGORB();

  nsresult statusForStop = aStatusCode;

  if (mState == State::Blocked && NS_FAILED(mStatus)) {
    statusForStop = mStatus;
  }

  return mNext->OnStopRequest(aRequest, statusForStop);
}

NS_IMETHODIMP
OpaqueResponseBlocker::OnDataAvailable(nsIRequest* aRequest,
                                       nsIInputStream* aInputStream,
                                       uint64_t aOffset, uint32_t aCount) {
  LOGORB();

  MOZ_ASSERT(mState == State::Allowed || mState == State::Blocked);
  if (mState == State::Allowed) {
    return mNext->OnDataAvailable(aRequest, aInputStream, aOffset, aCount);
  }

  if (mState == State::Blocked) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult OpaqueResponseBlocker::EnsureOpaqueResponseIsAllowedAfterSniff(
    nsIRequest* aRequest) {
  nsCOMPtr<HttpBaseChannel> httpBaseChannel = do_QueryInterface(aRequest);
  MOZ_ASSERT(httpBaseChannel);

  if (mState != State::Sniffing) {
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
    case OpaqueResponse::Alllow:
      AllowResponse();
      return NS_OK;
    case OpaqueResponse::Sniff:
      break;
  }

  return ValidateJavaScript(httpBaseChannel);
}

// The specification for ORB is currently being written:
// https://whatpr.org/fetch/1442.html#orb-algorithm
// The `opaque-response-safelist check` is implemented in:
// * `HttpBaseChannel::OpaqueResponseSafelistCheckBeforeSniff`
// * `nsHttpChannel::DisableIsOpaqueResponseAllowedAfterSniffCheck`
// * `HttpBaseChannel::OpaqueResponseSafelistCheckAfterSniff`
// * `OpaqueResponseBlocker::ValidateJavaScript`
nsresult OpaqueResponseBlocker::ValidateJavaScript(HttpBaseChannel* aChannel) {
  MOZ_DIAGNOSTIC_ASSERT(aChannel);
  int64_t contentLength;
  nsresult rv = aChannel->GetContentLength(&contentLength);
  if (NS_FAILED(rv)) {
    LOGORB("Blocked: No Content Length");
    BlockResponse(aChannel, rv);
    return rv;
  }

  // XXX(farre): this intentionally allowing responses, because we don't know
  // how to block the correct ones until bug 1532644 is completed.
  LOGORB("Allowed (After Sniff): passes all checks");
  AllowResponse();

  // https://whatpr.org/fetch/1442.html#orb-algorithm, step 15
  // XXX(farre): Start JavaScript validation.

  return NS_OK;
}

bool OpaqueResponseBlocker::IsSniffing() const {
  return mState == State::Sniffing;
}

void OpaqueResponseBlocker::AllowResponse() {
  LOGORB("Sniffer is done, allow response, this=%p", this);
  mState = State::Allowed;
}

void OpaqueResponseBlocker::BlockResponse(HttpBaseChannel* aChannel,
                                          nsresult aReason) {
  LOGORB("Sniffer is done, block response, this=%p", this);
  mState = State::Blocked;
  mStatus = aReason;
  aChannel->SetChannelBlockedByOpaqueResponse();
  aChannel->CancelWithReason(mStatus,
                             "OpaqueResponseBlocker::BlockResponse"_ns);
}

NS_IMPL_ISUPPORTS(OpaqueResponseBlocker, nsIStreamListener, nsIRequestObserver)

}  // namespace mozilla::net

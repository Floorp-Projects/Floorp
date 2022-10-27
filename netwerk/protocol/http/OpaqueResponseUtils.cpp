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
    const nsHttpResponseHead& aResponseHead) {
  nsAutoCString contentType;
  aResponseHead.ContentType(contentType);
  if (contentType.IsEmpty()) {
    return OpaqueResponseBlockedReason::BLOCKED_SHOULD_SNIFF;
  }

  if (IsOpaqueSafeListedMIMEType(contentType)) {
    return OpaqueResponseBlockedReason::ALLOWED_SAFE_LISTED;
  }

  if (IsOpaqueBlockListedNeverSniffedMIMEType(contentType)) {
    return OpaqueResponseBlockedReason::BLOCKED_BLOCKLISTED_NEVER_SNIFFED;
  }

  if (aResponseHead.Status() == 206 &&
      IsOpaqueBlockListedMIMEType(contentType)) {
    return OpaqueResponseBlockedReason::BLOCKED_206_AND_BLOCKLISTED;
  }

  nsAutoCString contentTypeOptionsHeader;
  if (aResponseHead.GetContentTypeOptionsHeader(contentTypeOptionsHeader) &&
      contentTypeOptionsHeader.EqualsIgnoreCase("nosniff") &&
      (IsOpaqueBlockListedMIMEType(contentType) ||
       contentType.EqualsLiteral(TEXT_PLAIN))) {
    return OpaqueResponseBlockedReason::
        BLOCKED_NOSNIFF_AND_EITHER_BLOCKLISTED_OR_TEXTPLAIN;
  }

  return OpaqueResponseBlockedReason::BLOCKED_SHOULD_SNIFF;
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
                                             HttpBaseChannel* aChannel)
    : mNext(aNext) {
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

  // When OnStartRequest is called, the UnknownDecoder has determined
  // its type already, so we should be able to make a decision
  if (mState == State::Sniffing) {
    MaybeORBSniff(aRequest);
  }

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
void OpaqueResponseBlocker::MaybeORBSniff(nsIRequest* aRequest) {
  if (!mCheckIsOpaqueResponseAllowedAfterSniff) {
    LOGORB("(doesn't check)");
    return;
  }
  LOGORB("(checks)");

  mCheckIsOpaqueResponseAllowedAfterSniff = false;

  nsCOMPtr<nsILoadInfo> loadInfo;
  nsCOMPtr<HttpBaseChannel> httpBaseChannel = do_QueryInterface(aRequest);
  MOZ_ASSERT(httpBaseChannel);

  nsresult rv =
      httpBaseChannel->GetLoadInfo(getter_AddRefs<nsILoadInfo>(loadInfo));
  if (NS_FAILED(rv)) {
    BlockResponse(httpBaseChannel, rv);
    return;
  }

  nsCOMPtr<nsIURI> uri;
  rv = httpBaseChannel->GetURI(getter_AddRefs<nsIURI>(uri));
  if (NS_FAILED(rv)) {
    LOGORB("Failed to get uri");
    BlockResponse(httpBaseChannel, rv);
    return;
  }

  bool isMediaRequest;
  loadInfo->GetIsMediaRequest(&isMediaRequest);
  if (isMediaRequest) {
    LogORBError(loadInfo, uri);
    BlockResponse(httpBaseChannel, NS_ERROR_FAILURE);
    return;
  }

  nsAutoCString contentType;
  rv = httpBaseChannel->GetContentType(contentType);
  if (NS_FAILED(rv)) {
    LogORBError(loadInfo, uri);
    BlockResponse(httpBaseChannel, rv);
    return;
  }

  nsHttpResponseHead* responseHead = httpBaseChannel->GetResponseHead();
  if (!responseHead) {
    AllowResponse();
    return;
  }

  nsAutoCString contentTypeOptionsHeader;
  if (responseHead->GetContentTypeOptionsHeader(contentTypeOptionsHeader) &&
      contentTypeOptionsHeader.EqualsIgnoreCase("nosniff")) {
    LOGORB("Blocked (After Sniff): nosniff");
    LogORBError(loadInfo, uri);
    BlockResponse(httpBaseChannel, NS_ERROR_FAILURE);
    return;
  }

  if (responseHead->Status() < 200 || responseHead->Status() > 299) {
    LOGORB("Blocked (After Sniff): status code (%d) is not allowed.",
           responseHead->Status());
    LogORBError(loadInfo, uri);
    BlockResponse(httpBaseChannel, NS_ERROR_FAILURE);
    return;
  }

  if (StringBeginsWith(contentType, "image/"_ns) ||
      StringBeginsWith(contentType, "video/"_ns) ||
      StringBeginsWith(contentType, "audio/"_ns)) {
    LOGORB("Blocked (After Sniff): content type image/video/audio");
    LogORBError(loadInfo, uri);
    BlockResponse(httpBaseChannel, NS_ERROR_FAILURE);
    return;
  }

  int64_t contentLength;
  rv = httpBaseChannel->GetContentLength(&contentLength);
  if (NS_FAILED(rv)) {
    LOGORB("Blocked (After Sniff): failed to get content length");
    LogORBError(loadInfo, uri);
    BlockResponse(httpBaseChannel, NS_ERROR_FAILURE);
    return;
  }

  LOGORB("Allowed (After Sniff): passes all checks");
  AllowResponse();
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

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/OpaqueResponseUtils.h"

#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "nsHttpResponseHead.h"
#include "nsMimeTypes.h"

namespace mozilla {
namespace net {

bool IsOpaqueSafeListedMIMEType(const nsACString& aContentType) {
  if (aContentType.EqualsLiteral(TEXT_CSS) ||
      aContentType.EqualsLiteral(IMAGE_SVG_XML)) {
    return true;
  }

  NS_ConvertUTF8toUTF16 typeString(aContentType);
  return nsContentUtils::IsJavascriptMIMEType(typeString);
}

bool IsOpaqueBlockListedMIMEType(const nsACString& aContentType) {
  return aContentType.EqualsLiteral(TEXT_HTML) ||
         StringEndsWith(aContentType, "+json"_ns) ||
         aContentType.EqualsLiteral(APPLICATION_JSON) ||
         aContentType.EqualsLiteral(TEXT_JSON) ||
         StringEndsWith(aContentType, "+xml"_ns) ||
         aContentType.EqualsLiteral(APPLICATION_XML) ||
         aContentType.EqualsLiteral(TEXT_XML);
}

bool IsOpaqueBlockListedNeverSniffedMIMEType(const nsACString& aContentType) {
  return aContentType.EqualsLiteral(APPLICATION_GZIP2) ||
         aContentType.EqualsLiteral(APPLICATION_MSEXCEL) ||
         aContentType.EqualsLiteral(APPLICATION_MSPPT) ||
         aContentType.EqualsLiteral(APPLICATION_MSWORD) ||
         aContentType.EqualsLiteral(APPLICATION_MSWORD_TEMPLATE) ||
         aContentType.EqualsLiteral(APPLICATION_PDF) ||
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
         aContentType.EqualsLiteral(APPLICATION_ZIP) ||
         aContentType.EqualsLiteral(MULTIPART_BYTERANGES) ||
         aContentType.EqualsLiteral(MULTIPART_SIGNED) ||
         aContentType.EqualsLiteral(TEXT_EVENT_STREAM) ||
         aContentType.EqualsLiteral(TEXT_CSV);
}

Result<std::tuple<int64_t, int64_t, int64_t>, nsresult>
ParseContentRangeHeaderString(const nsAutoCString& aRangeStr) {
  // Parse the range header: e.g. Content-Range: bytes 7000-7999/8000.
  const int32_t spacePos = aRangeStr.Find(" "_ns);
  const int32_t dashPos = aRangeStr.Find("-"_ns, true, spacePos);
  const int32_t slashPos = aRangeStr.Find("/"_ns, true, dashPos);

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
  if (rangeStart >= rangeEnd) {
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

OpaqueResponseBlockingInfo::OpaqueResponseBlockingInfo(
    ExtContentPolicyType aContentPolicyType)
    : mStartTime(TimeStamp::Now()) {
  switch (aContentPolicyType) {
    case ExtContentPolicy::TYPE_OTHER:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Other;
      break;
    case ExtContentPolicy::TYPE_SCRIPT:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Script;
      break;
    case ExtContentPolicy::TYPE_IMAGE:
    case ExtContentPolicy::TYPE_IMAGESET:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Image;
      break;
    case ExtContentPolicy::TYPE_STYLESHEET:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Style;
      break;
    case ExtContentPolicy::TYPE_OBJECT:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Object;
      break;
    case ExtContentPolicy::TYPE_DOCUMENT:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Document;
      break;
    case ExtContentPolicy::TYPE_SUBDOCUMENT:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Subdocument;
      break;
    case ExtContentPolicy::TYPE_PING:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Ping;
      break;
    case ExtContentPolicy::TYPE_XMLHTTPREQUEST:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::XHR;
      break;
    case ExtContentPolicy::TYPE_OBJECT_SUBREQUEST:
      mDestination =
          Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::ObjectSubrequest;
      break;
    case ExtContentPolicy::TYPE_DTD:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::DTD;
      break;
    case ExtContentPolicy::TYPE_FONT:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Font;
      break;
    case ExtContentPolicy::TYPE_MEDIA:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Media;
      break;
    case ExtContentPolicy::TYPE_WEBSOCKET:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Websocket;
      break;
    case ExtContentPolicy::TYPE_CSP_REPORT:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::CspReport;
      break;
    case ExtContentPolicy::TYPE_XSLT:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::XSLT;
      break;
    case ExtContentPolicy::TYPE_BEACON:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Beacon;
      break;
    case ExtContentPolicy::TYPE_FETCH:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Fetch;
      break;
    case ExtContentPolicy::TYPE_WEB_MANIFEST:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::WebManifest;
      break;
    default:
      mDestination = Telemetry::LABELS_OPAQUE_RESPONSE_BLOCKING::Unexpected;
      break;
  }
}

void OpaqueResponseBlockingInfo::Report(const nsCString& aKey) {
  Telemetry::AccumulateCategoricalKeyed(aKey, mDestination);

  AccumulateTimeDelta(Telemetry::OPAQUE_RESPONSE_BLOCKING_TIME_MS, mStartTime);
}

void OpaqueResponseBlockingInfo::ReportContentLength(int64_t aContentLength) {
  // XXX: We might want to filter negative cases (when the content length is
  // unknown).
  Telemetry::ScalarAdd(
      Telemetry::ScalarID::OPAQUE_RESPONSE_BLOCKING_PARSING_SIZE_KB,
      aContentLength > 0 ? aContentLength >> 10 : aContentLength);
}
}  // namespace net
}  // namespace mozilla

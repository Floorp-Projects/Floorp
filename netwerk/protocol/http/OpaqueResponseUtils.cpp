/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/OpaqueResponseUtils.h"

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

}  // namespace net
}  // namespace mozilla

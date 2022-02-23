/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtom.h"
#include "CParserContext.h"
#include "prenv.h"
#include "nsIHTMLContentSink.h"
#include "nsMimeTypes.h"

CParserContext::CParserContext(nsIURI* aURI, eParserCommands aCommand)
    : mScanner(aURI),
      mDTDMode(eDTDMode_autodetect),
      mDocType(eUnknown),
      mStreamListenerState(eNone),
      mContextType(eCTURL),
      mParserCommand(aCommand),
      mMultipart(true),
      mCopyUnused(false) {
  MOZ_COUNT_CTOR(CParserContext);
}

CParserContext::CParserContext(const nsAString& aBuffer,
                               eParserCommands aCommand, bool aLastBuffer)
    : mScanner(aBuffer, !aLastBuffer),
      mMimeType("application/xml"_ns),
      mDTDMode(eDTDMode_full_standards),
      mDocType(eXML),
      mStreamListenerState(aLastBuffer ? eOnStop : eOnDataAvail),
      mContextType(eCTString),
      mParserCommand(aCommand),
      mMultipart(!aLastBuffer),
      mCopyUnused(aLastBuffer) {
  MOZ_COUNT_CTOR(CParserContext);
}

CParserContext::~CParserContext() {
  // It's ok to simply ingore the PrevContext.
  MOZ_COUNT_DTOR(CParserContext);
}

void CParserContext::SetMimeType(const nsACString& aMimeType) {
  mMimeType.Assign(aMimeType);

  mDocType = eUnknown;

  if (mMimeType.EqualsLiteral(TEXT_HTML))
    mDocType = eHTML_Strict;
  else if (mMimeType.EqualsLiteral(TEXT_XML) ||
           mMimeType.EqualsLiteral(APPLICATION_XML) ||
           mMimeType.EqualsLiteral(APPLICATION_XHTML_XML) ||
           mMimeType.EqualsLiteral(IMAGE_SVG_XML) ||
           mMimeType.EqualsLiteral(APPLICATION_MATHML_XML) ||
           mMimeType.EqualsLiteral(APPLICATION_RDF_XML) ||
           mMimeType.EqualsLiteral(APPLICATION_WAPXHTML_XML) ||
           mMimeType.EqualsLiteral(TEXT_RDF))
    mDocType = eXML;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtom.h"
#include "CParserContext.h"
#include "prenv.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLTokenizer.h"
#include "nsMimeTypes.h"
#include "nsHTMLTokenizer.h"

CParserContext::CParserContext(CParserContext* aPrevContext,
                               nsScanner* aScanner, void* aKey,
                               eParserCommands aCommand,
                               eAutoDetectResult aStatus, bool aCopyUnused)
    : mKey(aKey),
      mPrevContext(aPrevContext),
      mScanner(mozilla::WrapUnique(aScanner)),
      mDTDMode(eDTDMode_unknown),
      mDocType(static_cast<eParserDocType>(0)),
      mStreamListenerState(eNone),
      mContextType(eCTNone),
      mAutoDetectStatus(aStatus),
      mParserCommand(aCommand),
      mMultipart(true),
      mCopyUnused(aCopyUnused) {
  MOZ_COUNT_CTOR(CParserContext);
}

CParserContext::~CParserContext() {
  // It's ok to simply ingore the PrevContext.
  MOZ_COUNT_DTOR(CParserContext);
}

void CParserContext::SetMimeType(const nsACString& aMimeType) {
  mMimeType.Assign(aMimeType);

  mDocType = ePlainText;

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

nsresult CParserContext::GetTokenizer(nsIDTD* aDTD, nsIContentSink* aSink,
                                      nsITokenizer*& aTokenizer) {
  nsresult result = NS_OK;
  int32_t type = aDTD ? aDTD->GetType() : NS_IPARSER_FLAG_HTML;

  if (!mTokenizer) {
    if (type == NS_IPARSER_FLAG_HTML || mParserCommand == eViewSource) {
      mTokenizer = new nsHTMLTokenizer;
    } else if (type == NS_IPARSER_FLAG_XML) {
      mTokenizer = do_QueryInterface(aDTD, &result);
    }
  }

  aTokenizer = mTokenizer;

  return result;
}

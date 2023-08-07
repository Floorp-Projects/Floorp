/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5DocumentBuilder.h"

#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/LinkStyle.h"
#include "nsNameSpaceManager.h"

using mozilla::dom::LinkStyle;

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsHtml5DocumentBuilder, nsContentSink,
                                   mOwnedElements)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsHtml5DocumentBuilder)
NS_INTERFACE_MAP_END_INHERITING(nsContentSink)

NS_IMPL_ADDREF_INHERITED(nsHtml5DocumentBuilder, nsContentSink)
NS_IMPL_RELEASE_INHERITED(nsHtml5DocumentBuilder, nsContentSink)

nsHtml5DocumentBuilder::nsHtml5DocumentBuilder(bool aRunsToCompletion)
    : mBroken(NS_OK), mFlushState(eHtml5FlushState::eNotFlushing) {
  mRunsToCompletion = aRunsToCompletion;
}

nsresult nsHtml5DocumentBuilder::Init(mozilla::dom::Document* aDoc,
                                      nsIURI* aURI, nsISupports* aContainer,
                                      nsIChannel* aChannel) {
  return nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
}

nsHtml5DocumentBuilder::~nsHtml5DocumentBuilder() = default;

nsresult nsHtml5DocumentBuilder::MarkAsBroken(nsresult aReason) {
  mBroken = aReason;
  return aReason;
}

void nsHtml5DocumentBuilder::UpdateStyleSheet(nsIContent* aElement) {
  auto* linkStyle = LinkStyle::FromNode(*aElement);
  if (!linkStyle) {
    MOZ_ASSERT(nsNameSpaceManager::GetInstance()->mSVGDisabled,
               "Node didn't QI to style, but SVG wasn't disabled.");
    return;
  }

  auto updateOrError = linkStyle->EnableUpdatesAndUpdateStyleSheet(
      mRunsToCompletion ? nullptr : this);

  if (updateOrError.isOk() && updateOrError.unwrap().ShouldBlock() &&
      !mRunsToCompletion) {
    ++mPendingSheetCount;
    mScriptLoader->AddParserBlockingScriptExecutionBlocker();
  }
}

void nsHtml5DocumentBuilder::SetDocumentMode(nsHtml5DocumentMode m) {
  nsCompatibility mode = eCompatibility_NavQuirks;
  const char* errMsgId = nullptr;

  switch (m) {
    case STANDARDS_MODE:
      mode = eCompatibility_FullStandards;
      break;
    case ALMOST_STANDARDS_MODE:
      mode = eCompatibility_AlmostStandards;
      errMsgId = "errAlmostStandardsDoctypeVerbose";
      break;
    case QUIRKS_MODE:
      mode = eCompatibility_NavQuirks;
      errMsgId = "errQuirkyDoctypeVerbose";
      break;
  }
  mDocument->SetCompatibilityMode(mode);

  if (errMsgId && !mDocument->IsLoadedAsData()) {
    nsCOMPtr<nsIURI> docURI = mDocument->GetDocumentURI();
    bool isData = false;
    docURI->SchemeIs("data", &isData);
    bool isHttp = false;
    docURI->SchemeIs("http", &isHttp);
    bool isHttps = false;
    docURI->SchemeIs("https", &isHttps);

    nsCOMPtr<nsIPrincipal> principal = mDocument->GetPrincipal();
    if (principal->GetIsNullPrincipal() && !isData && !isHttp && !isHttps) {
      // Don't normally warn for null principals. It may well be internal
      // documents for which the warning is not applicable.
      return;
    }

    nsContentUtils::ReportToConsole(
        nsIScriptError::warningFlag, "HTML_PARSER__DOCTYPE"_ns, mDocument,
        nsContentUtils::eHTMLPARSER_PROPERTIES, errMsgId);
  }
}

// nsContentSink overrides

void nsHtml5DocumentBuilder::UpdateChildCounts() {
  // No-op
}

nsresult nsHtml5DocumentBuilder::FlushTags() { return NS_OK; }

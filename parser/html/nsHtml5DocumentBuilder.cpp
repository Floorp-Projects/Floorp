/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5DocumentBuilder.h"

#include "mozilla/dom/ScriptLoader.h"
#include "nsIHTMLDocument.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsNameSpaceManager.h"
#include "nsStyleLinkElement.h"

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsHtml5DocumentBuilder,
                                   nsContentSink,
                                   mOwnedElements)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsHtml5DocumentBuilder)
NS_INTERFACE_MAP_END_INHERITING(nsContentSink)

NS_IMPL_ADDREF_INHERITED(nsHtml5DocumentBuilder, nsContentSink)
NS_IMPL_RELEASE_INHERITED(nsHtml5DocumentBuilder, nsContentSink)

nsHtml5DocumentBuilder::nsHtml5DocumentBuilder(bool aRunsToCompletion)
  : mBroken(NS_OK)
  , mFlushState(eHtml5FlushState::eNotFlushing)
{
  mRunsToCompletion = aRunsToCompletion;
}

nsresult
nsHtml5DocumentBuilder::Init(nsIDocument* aDoc,
                             nsIURI* aURI,
                             nsISupports* aContainer,
                             nsIChannel* aChannel)
{
  return nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
}

nsHtml5DocumentBuilder::~nsHtml5DocumentBuilder() {}

nsresult
nsHtml5DocumentBuilder::MarkAsBroken(nsresult aReason)
{
  mBroken = aReason;
  return aReason;
}

void
nsHtml5DocumentBuilder::SetDocumentCharsetAndSource(
  NotNull<const Encoding*> aEncoding,
  int32_t aCharsetSource)
{
  if (mDocument) {
    mDocument->SetDocumentCharacterSetSource(aCharsetSource);
    mDocument->SetDocumentCharacterSet(aEncoding);
  }
}

void
nsHtml5DocumentBuilder::UpdateStyleSheet(nsIContent* aElement)
{
  nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(aElement));
  if (!ssle) {
    MOZ_ASSERT(nsNameSpaceManager::GetInstance()->mSVGDisabled,
               "Node didn't QI to style, but SVG wasn't disabled.");
    return;
  }

  // Break out of the doc update created by Flush() to zap a runnable
  // waiting to call UpdateStyleSheet without the right observer
  EndDocUpdate();

  if (MOZ_UNLIKELY(!mParser)) {
    // EndDocUpdate ran stuff that called nsIParser::Terminate()
    return;
  }

  ssle->SetEnableUpdates(true);

  auto updateOrError =
    ssle->UpdateStyleSheet(mRunsToCompletion ? nullptr : this);

  if (updateOrError.isOk() &&
      updateOrError.unwrap().ShouldBlock() &&
      !mRunsToCompletion) {
    ++mPendingSheetCount;
    mScriptLoader->AddParserBlockingScriptExecutionBlocker();
  }

  // Re-open update
  BeginDocUpdate();
}

void
nsHtml5DocumentBuilder::SetDocumentMode(nsHtml5DocumentMode m)
{
  nsCompatibility mode = eCompatibility_NavQuirks;
  switch (m) {
    case STANDARDS_MODE:
      mode = eCompatibility_FullStandards;
      break;
    case ALMOST_STANDARDS_MODE:
      mode = eCompatibility_AlmostStandards;
      break;
    case QUIRKS_MODE:
      mode = eCompatibility_NavQuirks;
      break;
  }
  nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(mDocument);
  NS_ASSERTION(htmlDocument, "Document didn't QI into HTML document.");
  htmlDocument->SetCompatibilityMode(mode);
}

// nsContentSink overrides

void
nsHtml5DocumentBuilder::UpdateChildCounts()
{
  // No-op
}

nsresult
nsHtml5DocumentBuilder::FlushTags()
{
  return NS_OK;
}

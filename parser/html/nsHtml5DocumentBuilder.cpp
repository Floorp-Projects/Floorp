/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5DocumentBuilder.h"

#include "nsIStyleSheetLinkingElement.h"
#include "nsStyleLinkElement.h"
#include "nsScriptLoader.h"
#include "nsIHTMLDocument.h"

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(nsHtml5DocumentBuilder, nsContentSink,
                                     mOwnedElements)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsHtml5DocumentBuilder)
NS_INTERFACE_MAP_END_INHERITING(nsContentSink)

NS_IMPL_ADDREF_INHERITED(nsHtml5DocumentBuilder, nsContentSink)
NS_IMPL_RELEASE_INHERITED(nsHtml5DocumentBuilder, nsContentSink)

nsHtml5DocumentBuilder::nsHtml5DocumentBuilder(bool aRunsToCompletion)
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

nsHtml5DocumentBuilder::~nsHtml5DocumentBuilder()
{
}

nsresult
nsHtml5DocumentBuilder::MarkAsBroken(nsresult aReason)
{
  mBroken = aReason;
  return aReason;
}

void
nsHtml5DocumentBuilder::SetDocumentCharsetAndSource(nsACString& aCharset, int32_t aCharsetSource)
{
  if (mDocument) {
    mDocument->SetDocumentCharacterSetSource(aCharsetSource);
    mDocument->SetDocumentCharacterSet(aCharset);
  }
}

void
nsHtml5DocumentBuilder::UpdateStyleSheet(nsIContent* aElement)
{
  // Break out of the doc update created by Flush() to zap a runnable
  // waiting to call UpdateStyleSheet without the right observer
  EndDocUpdate();

  if (MOZ_UNLIKELY(!mParser)) {
    // EndDocUpdate ran stuff that called nsIParser::Terminate()
    return;
  }

  nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(aElement));
  NS_ASSERTION(ssle, "Node didn't QI to style.");

  ssle->SetEnableUpdates(true);

  bool willNotify;
  bool isAlternate;
  nsresult rv = ssle->UpdateStyleSheet(mRunsToCompletion ? nullptr : this,
                                       &willNotify,
                                       &isAlternate);
  if (NS_SUCCEEDED(rv) && willNotify && !isAlternate && !mRunsToCompletion) {
    ++mPendingSheetCount;
    mScriptLoader->AddExecuteBlocker();
  }

  if (aElement->IsHTML(nsGkAtoms::link)) {
    // look for <link rel="next" href="url">
    nsAutoString relVal;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, relVal);
    if (!relVal.IsEmpty()) {
      uint32_t linkTypes = nsStyleLinkElement::ParseLinkTypes(relVal);
      bool hasPrefetch = linkTypes & nsStyleLinkElement::ePREFETCH;
      if (hasPrefetch || (linkTypes & nsStyleLinkElement::eNEXT)) {
        nsAutoString hrefVal;
        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::href, hrefVal);
        if (!hrefVal.IsEmpty()) {
          PrefetchHref(hrefVal, aElement, hasPrefetch);
        }
      }
      if (linkTypes & nsStyleLinkElement::eDNS_PREFETCH) {
        nsAutoString hrefVal;
        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::href, hrefVal);
        if (!hrefVal.IsEmpty()) {
          PrefetchDNS(hrefVal);
        }
      }
    }
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

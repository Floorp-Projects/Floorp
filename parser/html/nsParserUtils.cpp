/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsParserUtils.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptLoader.h"
#include "nsAttrName.h"
#include "nsCOMPtr.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsEscape.h"
#include "nsHTMLParts.h"
#include "nsHtml5Module.h"
#include "nsIContent.h"
#include "nsIContentSink.h"
#include "nsIDTD.h"
#include "mozilla/dom/Document.h"
#include "nsIDocumentEncoder.h"
#include "nsIFragmentContentSink.h"
#include "nsIParser.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsTreeSanitizer.h"
#include "nsXPCOM.h"

#define XHTML_DIV_TAG u"div xmlns=\"http://www.w3.org/1999/xhtml\""

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsParserUtils, nsIParserUtils)

NS_IMETHODIMP
nsParserUtils::ConvertToPlainText(const nsAString& aFromStr, uint32_t aFlags,
                                  uint32_t aWrapCol, nsAString& aToStr) {
  return nsContentUtils::ConvertToPlainText(aFromStr, aToStr, aFlags, aWrapCol);
}

template <typename Callable>
static nsresult SanitizeWith(const nsAString& aInput, nsAString& aOutput,
                             Callable aDoSanitize) {
  RefPtr<Document> document = nsContentUtils::CreateInertHTMLDocument(nullptr);
  if (!document) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = nsContentUtils::ParseDocumentHTML(aInput, document, false);
  NS_ENSURE_SUCCESS(rv, rv);

  aDoSanitize(document.get());

  nsCOMPtr<nsIDocumentEncoder> encoder = do_createDocumentEncoder("text/html");
  encoder->NativeInit(document, u"text/html"_ns,
                      nsIDocumentEncoder::OutputDontRewriteEncodingDeclaration |
                          nsIDocumentEncoder::OutputNoScriptContent |
                          nsIDocumentEncoder::OutputEncodeBasicEntities |
                          nsIDocumentEncoder::OutputLFLineBreak |
                          nsIDocumentEncoder::OutputRaw);
  return encoder->EncodeToString(aOutput);
}

NS_IMETHODIMP
nsParserUtils::Sanitize(const nsAString& aFromStr, uint32_t aFlags,
                        nsAString& aToStr) {
  return SanitizeWith(aFromStr, aToStr, [&](Document* aDocument) {
    nsTreeSanitizer sanitizer(aFlags);
    sanitizer.Sanitize(aDocument);
  });
}

NS_IMETHODIMP
nsParserUtils::RemoveConditionalCSS(const nsAString& aFromStr,
                                    nsAString& aToStr) {
  return SanitizeWith(aFromStr, aToStr, [](Document* aDocument) {
    nsTreeSanitizer::RemoveConditionalCSSFromSubtree(aDocument);
  });
}

NS_IMETHODIMP
nsParserUtils::ParseFragment(const nsAString& aFragment, uint32_t aFlags,
                             bool aIsXML, nsIURI* aBaseURI,
                             Element* aContextElement,
                             DocumentFragment** aReturn) {
  NS_ENSURE_ARG(aContextElement);
  *aReturn = nullptr;

  RefPtr<Document> document = aContextElement->OwnerDoc();

  nsAutoScriptBlockerSuppressNodeRemoved autoBlocker;

  // stop scripts
  RefPtr<ScriptLoader> loader = document->ScriptLoader();
  bool scripts_enabled = loader->GetEnabled();
  if (scripts_enabled) {
    loader->SetEnabled(false);
  }

  // Wrap things in a div or body for parsing, but it won't show up in
  // the fragment.
  nsresult rv = NS_OK;
  AutoTArray<nsString, 2> tagStack;
  RefPtr<DocumentFragment> fragment;
  if (aIsXML) {
    // XHTML
    tagStack.AppendElement(nsLiteralString(XHTML_DIV_TAG));
    rv = nsContentUtils::ParseFragmentXML(aFragment, document, tagStack, true,
                                          aFlags, getter_AddRefs(fragment));
  } else {
    fragment = new (document->NodeInfoManager())
        DocumentFragment(document->NodeInfoManager());
    rv = nsContentUtils::ParseFragmentHTML(aFragment, fragment, nsGkAtoms::body,
                                           kNameSpaceID_XHTML, false, true,
                                           aFlags);
  }

  if (scripts_enabled) {
    loader->SetEnabled(true);
  }

  fragment.forget(aReturn);
  return rv;
}

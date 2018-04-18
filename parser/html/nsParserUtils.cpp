/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsParserUtils.h"
#include "NullPrincipal.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/ScriptLoader.h"
#include "nsAttrName.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsEscape.h"
#include "nsHTMLParts.h"
#include "nsHtml5Module.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsIContentSink.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDTD.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIFragmentContentSink.h"
#include "nsIParser.h"
#include "nsIScriptableUnescapeHTML.h"
#include "nsISupportsPrimitives.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsString.h"
#include "nsTreeSanitizer.h"
#include "nsXPCOM.h"

#define XHTML_DIV_TAG "div xmlns=\"http://www.w3.org/1999/xhtml\""

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsParserUtils, nsIScriptableUnescapeHTML, nsIParserUtils)

NS_IMETHODIMP
nsParserUtils::ConvertToPlainText(const nsAString& aFromStr,
                                  uint32_t aFlags,
                                  uint32_t aWrapCol,
                                  nsAString& aToStr)
{
  return nsContentUtils::ConvertToPlainText(aFromStr, aToStr, aFlags, aWrapCol);
}

NS_IMETHODIMP
nsParserUtils::Unescape(const nsAString& aFromStr, nsAString& aToStr)
{
  return nsContentUtils::ConvertToPlainText(
    aFromStr,
    aToStr,
    nsIDocumentEncoder::OutputSelectionOnly |
      nsIDocumentEncoder::OutputAbsoluteLinks,
    0);
}

NS_IMETHODIMP
nsParserUtils::Sanitize(const nsAString& aFromStr,
                        uint32_t aFlags,
                        nsAString& aToStr)
{
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:blank");
  nsCOMPtr<nsIPrincipal> principal = NullPrincipal::CreateWithoutOriginAttributes();
  nsCOMPtr<nsIDOMDocument> domDocument;
  nsresult rv = NS_NewDOMDocument(getter_AddRefs(domDocument),
                                  EmptyString(),
                                  EmptyString(),
                                  nullptr,
                                  uri,
                                  uri,
                                  principal,
                                  true,
                                  nullptr,
                                  DocumentFlavorHTML);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
  rv = nsContentUtils::ParseDocumentHTML(aFromStr, document, false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTreeSanitizer sanitizer(aFlags);
  sanitizer.Sanitize(document);

  nsCOMPtr<nsIDocumentEncoder> encoder =
    do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/html");

  encoder->NativeInit(document,
                      NS_LITERAL_STRING("text/html"),
                      nsIDocumentEncoder::OutputDontRewriteEncodingDeclaration |
                        nsIDocumentEncoder::OutputNoScriptContent |
                        nsIDocumentEncoder::OutputEncodeBasicEntities |
                        nsIDocumentEncoder::OutputLFLineBreak |
                        nsIDocumentEncoder::OutputRaw);

  return encoder->EncodeToString(aToStr);
}

NS_IMETHODIMP
nsParserUtils::ParseFragment(const nsAString& aFragment,
                             bool aIsXML,
                             nsIURI* aBaseURI,
                             nsIDOMElement* aContextElement,
                             DocumentFragment** aReturn)
{
  return nsParserUtils::ParseFragment(
    aFragment, 0, aIsXML, aBaseURI, aContextElement, aReturn);
}

NS_IMETHODIMP
nsParserUtils::ParseFragment(const nsAString& aFragment,
                             uint32_t aFlags,
                             bool aIsXML,
                             nsIURI* aBaseURI,
                             nsIDOMElement* aContextElement,
                             DocumentFragment** aReturn)
{
  NS_ENSURE_ARG(aContextElement);
  *aReturn = nullptr;

  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsINode> contextNode;
  contextNode = do_QueryInterface(aContextElement);
  document = contextNode->OwnerDoc();

  nsAutoScriptBlockerSuppressNodeRemoved autoBlocker;

  // stop scripts
  RefPtr<ScriptLoader> loader;
  bool scripts_enabled = false;
  if (document) {
    loader = document->ScriptLoader();
    scripts_enabled = loader->GetEnabled();
  }
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
    tagStack.AppendElement(NS_LITERAL_STRING(XHTML_DIV_TAG));
    rv = nsContentUtils::ParseFragmentXML(
      aFragment, document, tagStack, true, getter_AddRefs(fragment));
  } else {
    fragment = new DocumentFragment(document->NodeInfoManager());
    rv = nsContentUtils::ParseFragmentHTML(
      aFragment, fragment, nsGkAtoms::body, kNameSpaceID_XHTML, false, true);
  }
  if (fragment) {
    nsTreeSanitizer sanitizer(aFlags);
    sanitizer.Sanitize(fragment);
  }

  if (scripts_enabled) {
    loader->SetEnabled(true);
  }

  fragment.forget(aReturn);
  return rv;
}

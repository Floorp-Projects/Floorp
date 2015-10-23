/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonContentPolicy.h"

#include "nsCOMPtr.h"
#include "nsContentPolicyUtils.h"
#include "nsContentTypeParser.h"
#include "nsContentUtils.h"
#include "nsIConsoleService.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIScriptError.h"
#include "nsIURI.h"

/* Enforces content policies for WebExtension scopes. Currently:
 *
 *  - Prevents loading scripts with a non-default JavaScript version.
 */

#define VERSIONED_JS_BLOCKED_MESSAGE \
  MOZ_UTF16("Versioned JavaScript is a non-standard, deprecated extension, and is ") \
  MOZ_UTF16("not supported in WebExtension code. For alternatives, please see: ") \
  MOZ_UTF16("https://developer.mozilla.org/Add-ons/WebExtensions/Tips")

AddonContentPolicy::AddonContentPolicy()
{
}

AddonContentPolicy::~AddonContentPolicy()
{
}

NS_IMPL_ISUPPORTS(AddonContentPolicy, nsIContentPolicy)

static nsresult
GetWindowIDFromContext(nsISupports* aContext, uint64_t *aResult)
{
  NS_ENSURE_TRUE(aContext, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> document = content->OwnerDoc();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindow> window = document->GetInnerWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  *aResult = window->WindowID();
  return NS_OK;
}

static nsresult
LogMessage(const nsAString &aMessage, nsIURI* aSourceURI, const nsAString &aSourceSample,
           nsISupports* aContext)
{
  nsCOMPtr<nsIScriptError> error = do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  NS_ENSURE_TRUE(error, NS_ERROR_OUT_OF_MEMORY);

  nsAutoCString sourceName;
  nsresult rv = aSourceURI->GetSpec(sourceName);
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t windowID = 0;
  GetWindowIDFromContext(aContext, &windowID);

  rv = error->InitWithWindowID(aMessage, NS_ConvertUTF8toUTF16(sourceName),
                               aSourceSample, 0, 0, nsIScriptError::errorFlag,
                               "JavaScript", windowID);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIConsoleService> console = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_ENSURE_TRUE(console, NS_ERROR_OUT_OF_MEMORY);

  console->LogMessage(error);
  return NS_OK;
}

NS_IMETHODIMP
AddonContentPolicy::ShouldLoad(uint32_t aContentType,
                               nsIURI* aContentLocation,
                               nsIURI* aRequestOrigin,
                               nsISupports* aContext,
                               const nsACString& aMimeTypeGuess,
                               nsISupports* aExtra,
                               nsIPrincipal* aRequestPrincipal,
                               int16_t* aShouldLoad)
{
  MOZ_ASSERT(aContentType == nsContentUtils::InternalContentPolicyTypeToExternal(aContentType),
             "We should only see external content policy types here.");

  *aShouldLoad = nsIContentPolicy::ACCEPT;

  if (!aRequestOrigin) {
    return NS_OK;
  }

  // Only apply this policy to requests from documents loaded from
  // moz-extension URLs, or to resources being loaded from moz-extension URLs.
  bool equals;
  if (!((NS_SUCCEEDED(aContentLocation->SchemeIs("moz-extension", &equals)) && equals) ||
        (NS_SUCCEEDED(aRequestOrigin->SchemeIs("moz-extension", &equals)) && equals))) {
    return NS_OK;
  }

  if (aContentType == nsIContentPolicy::TYPE_SCRIPT) {
    NS_ConvertUTF8toUTF16 typeString(aMimeTypeGuess);
    nsContentTypeParser mimeParser(typeString);

    // Reject attempts to load JavaScript scripts with a non-default version.
    nsAutoString mimeType, version;
    if (NS_SUCCEEDED(mimeParser.GetType(mimeType)) &&
        nsContentUtils::IsJavascriptMIMEType(mimeType) &&
        NS_SUCCEEDED(mimeParser.GetParameter("version", version))) {
      *aShouldLoad = nsIContentPolicy::REJECT_REQUEST;

      LogMessage(NS_MULTILINE_LITERAL_STRING(VERSIONED_JS_BLOCKED_MESSAGE),
                 aRequestOrigin, typeString, aContext);
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
AddonContentPolicy::ShouldProcess(uint32_t aContentType,
                                  nsIURI* aContentLocation,
                                  nsIURI* aRequestOrigin,
                                  nsISupports* aRequestingContext,
                                  const nsACString& aMimeTypeGuess,
                                  nsISupports* aExtra,
                                  nsIPrincipal* aRequestPrincipal,
                                  int16_t* aShouldProcess)
{
  MOZ_ASSERT(aContentType == nsContentUtils::InternalContentPolicyTypeToExternal(aContentType),
             "We should only see external content policy types here.");

  *aShouldProcess = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

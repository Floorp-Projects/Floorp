/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AddonContentPolicy.h"

#include "mozilla/dom/nsCSPContext.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsContentTypeParser.h"
#include "nsContentUtils.h"
#include "nsIConsoleService.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIContent.h"
#include "mozilla/Components.h"
#include "mozilla/dom/Document.h"
#include "nsIEffectiveTLDService.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsIUUIDGenerator.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"

using namespace mozilla;

/* Enforces content policies for WebExtension scopes. Currently:
 *
 *  - Prevents loading scripts with a non-default JavaScript version.
 *  - Checks custom content security policies for sufficiently stringent
 *    script-src and object-src directives.
 */

#define VERSIONED_JS_BLOCKED_MESSAGE                                       \
  u"Versioned JavaScript is a non-standard, deprecated extension, and is " \
  u"not supported in WebExtension code. For alternatives, please see: "    \
  u"https://developer.mozilla.org/Add-ons/WebExtensions/Tips"

AddonContentPolicy::AddonContentPolicy() = default;

AddonContentPolicy::~AddonContentPolicy() = default;

NS_IMPL_ISUPPORTS(AddonContentPolicy, nsIContentPolicy, nsIAddonContentPolicy)

static nsresult GetWindowIDFromContext(nsISupports* aContext,
                                       uint64_t* aResult) {
  NS_ENSURE_TRUE(aContext, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindowInner> window = content->OwnerDoc()->GetInnerWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  *aResult = window->WindowID();
  return NS_OK;
}

static nsresult LogMessage(const nsAString& aMessage,
                           const nsAString& aSourceName,
                           const nsAString& aSourceSample,
                           nsISupports* aContext) {
  nsCOMPtr<nsIScriptError> error = do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  NS_ENSURE_TRUE(error, NS_ERROR_OUT_OF_MEMORY);

  uint64_t windowID = 0;
  GetWindowIDFromContext(aContext, &windowID);

  nsresult rv = error->InitWithSanitizedSource(
      aMessage, aSourceName, aSourceSample, 0, 0, nsIScriptError::errorFlag,
      "JavaScript", windowID);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIConsoleService> console =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_ENSURE_TRUE(console, NS_ERROR_OUT_OF_MEMORY);

  console->LogMessage(error);
  return NS_OK;
}

// Content policy enforcement:

NS_IMETHODIMP
AddonContentPolicy::ShouldLoad(nsIURI* aContentLocation, nsILoadInfo* aLoadInfo,
                               const nsACString& aMimeTypeGuess,
                               int16_t* aShouldLoad) {
  if (!aContentLocation || !aLoadInfo) {
    NS_SetRequestBlockingReason(
        aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_WEBEXT);
    *aShouldLoad = REJECT_REQUEST;
    return NS_ERROR_FAILURE;
  }

  ExtContentPolicyType contentType = aLoadInfo->GetExternalContentPolicyType();

  *aShouldLoad = nsIContentPolicy::ACCEPT;
  nsCOMPtr<nsIPrincipal> loadingPrincipal = aLoadInfo->GetLoadingPrincipal();
  if (!loadingPrincipal) {
    return NS_OK;
  }

  // Only apply this policy to requests from documents loaded from
  // moz-extension URLs, or to resources being loaded from moz-extension URLs.
  if (!(aContentLocation->SchemeIs("moz-extension") ||
        loadingPrincipal->SchemeIs("moz-extension"))) {
    return NS_OK;
  }

  if (contentType == ExtContentPolicy::TYPE_SCRIPT) {
    NS_ConvertUTF8toUTF16 typeString(aMimeTypeGuess);
    nsContentTypeParser mimeParser(typeString);

    // Reject attempts to load JavaScript scripts with a non-default version.
    nsAutoString mimeType, version;
    if (NS_SUCCEEDED(mimeParser.GetType(mimeType)) &&
        nsContentUtils::IsJavascriptMIMEType(mimeType) &&
        NS_SUCCEEDED(mimeParser.GetParameter("version", version))) {
      NS_SetRequestBlockingReason(
          aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_WEBEXT);
      *aShouldLoad = nsIContentPolicy::REJECT_REQUEST;

      nsCString sourceName;
      loadingPrincipal->GetExposableSpec(sourceName);
      NS_ConvertUTF8toUTF16 nameString(sourceName);

      nsCOMPtr<nsISupports> context = aLoadInfo->GetLoadingContext();
      LogMessage(nsLiteralString(VERSIONED_JS_BLOCKED_MESSAGE), nameString,
                 typeString, context);
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
AddonContentPolicy::ShouldProcess(nsIURI* aContentLocation,
                                  nsILoadInfo* aLoadInfo,
                                  const nsACString& aMimeTypeGuess,
                                  int16_t* aShouldProcess) {
  *aShouldProcess = nsIContentPolicy::ACCEPT;
  return NS_OK;
}

// CSP Validation:

static const char* allowedSchemes[] = {"blob", "filesystem", nullptr};

static const char* allowedHostSchemes[] = {"http", "https", "moz-extension",
                                           nullptr};

/**
 * Validates a CSP directive to ensure that it is sufficiently stringent.
 * In particular, ensures that:
 *
 *  - No remote sources are allowed other than from https: schemes
 *
 *  - No remote sources specify host wildcards for generic domains
 *    (*.blogspot.com, *.com, *)
 *
 *  - All remote sources and local extension sources specify a host
 *
 *  - No scheme sources are allowed other than blob:, filesystem:,
 *    moz-extension:, and https:
 *
 *  - No keyword sources are allowed other than 'none', 'self', 'unsafe-eval',
 *    and hash sources.
 *
 *  Manifest V3 limits CSP for extension_pages, the script-src, object-src, and
 *  worker-src directives may only be the following:
 *    - self
 *    - none
 *    - Any localhost source, (http://localhost, http://127.0.0.1, or any port
 * on those domains)
 */
class CSPValidator final : public nsCSPSrcVisitor {
 public:
  CSPValidator(nsAString& aURL, CSPDirective aDirective,
               bool aDirectiveRequired = true, uint32_t aPermittedPolicy = 0)
      : mURL(aURL),
        mDirective(CSP_CSPDirectiveToString(aDirective)),
        mPermittedPolicy(aPermittedPolicy),
        mFoundSelf(false) {
    // Start with the default error message for a missing directive, since no
    // visitors will be called if the directive isn't present.
    mError.SetIsVoid(true);
    if (aDirectiveRequired) {
      FormatError("csp.error.missing-directive");
    }
  }

  // Visitors

  bool visitSchemeSrc(const nsCSPSchemeSrc& src) override {
    nsAutoString scheme;
    src.getScheme(scheme);

    if (SchemeInList(scheme, allowedHostSchemes)) {
      FormatError("csp.error.missing-host", scheme);
      return false;
    }
    if (!SchemeInList(scheme, allowedSchemes)) {
      FormatError("csp.error.illegal-protocol", scheme);
      return false;
    }
    return true;
  };

  bool visitHostSrc(const nsCSPHostSrc& src) override {
    nsAutoString scheme, host;

    src.getScheme(scheme);
    src.getHost(host);

    if (scheme.LowerCaseEqualsLiteral("http")) {
      // Allow localhost on http
      if (mPermittedPolicy & nsIAddonContentPolicy::CSP_ALLOW_LOCALHOST &&
          HostIsLocal(host)) {
        return true;
      }
      FormatError("csp.error.illegal-protocol", scheme);
      return false;
    }
    if (scheme.LowerCaseEqualsLiteral("https")) {
      if (mPermittedPolicy & nsIAddonContentPolicy::CSP_ALLOW_LOCALHOST &&
          HostIsLocal(host)) {
        return true;
      }
      if (!(mPermittedPolicy & nsIAddonContentPolicy::CSP_ALLOW_REMOTE)) {
        FormatError("csp.error.illegal-protocol", scheme);
        return false;
      }
      if (!HostIsAllowed(host)) {
        FormatError("csp.error.illegal-host-wildcard", scheme);
        return false;
      }
    } else if (scheme.LowerCaseEqualsLiteral("moz-extension")) {
      // The CSP parser silently converts 'self' keywords to the origin
      // URL, so we need to reconstruct the URL to see if it was present.
      if (!mFoundSelf) {
        nsAutoString url(u"moz-extension://");
        url.Append(host);

        mFoundSelf = url.Equals(mURL);
      }

      if (host.IsEmpty() || host.EqualsLiteral("*")) {
        FormatError("csp.error.missing-host", scheme);
        return false;
      }
    } else if (!SchemeInList(scheme, allowedSchemes)) {
      FormatError("csp.error.illegal-protocol", scheme);
      return false;
    }

    return true;
  };

  bool visitKeywordSrc(const nsCSPKeywordSrc& src) override {
    switch (src.getKeyword()) {
      case CSP_NONE:
      case CSP_SELF:
        return true;
      case CSP_UNSAFE_EVAL:
        if (mPermittedPolicy & nsIAddonContentPolicy::CSP_ALLOW_EVAL) {
          return true;
        }
        // fall through and produce an illegal-keyword error.
        [[fallthrough]];
      default:
        FormatError(
            "csp.error.illegal-keyword",
            nsDependentString(CSP_EnumToUTF16Keyword(src.getKeyword())));
        return false;
    }
  };

  bool visitNonceSrc(const nsCSPNonceSrc& src) override {
    FormatError("csp.error.illegal-keyword", u"'nonce-*'"_ns);
    return false;
  };

  bool visitHashSrc(const nsCSPHashSrc& src) override { return true; };

  // Accessors

  inline nsAString& GetError() { return mError; };

  inline bool FoundSelf() { return mFoundSelf; };

  // Formatters

  template <typename... T>
  inline void FormatError(const char* aName, const T... aParams) {
    AutoTArray<nsString, sizeof...(aParams) + 1> params = {mDirective,
                                                           aParams...};
    FormatErrorParams(aName, params);
  };

 private:
  // Validators
  bool HostIsLocal(nsAString& host) {
    return host.EqualsLiteral("localhost") || host.EqualsLiteral("127.0.0.1");
  }

  bool HostIsAllowed(nsAString& host) {
    if (host.First() == '*') {
      if (host.EqualsLiteral("*") || host[1] != '.') {
        return false;
      }

      host.Cut(0, 2);

      nsCOMPtr<nsIEffectiveTLDService> tldService =
          do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);

      if (!tldService) {
        return false;
      }

      NS_ConvertUTF16toUTF8 cHost(host);
      nsAutoCString publicSuffix;

      nsresult rv = tldService->GetPublicSuffixFromHost(cHost, publicSuffix);

      return NS_SUCCEEDED(rv) && !cHost.Equals(publicSuffix);
    }

    return true;
  };

  bool SchemeInList(nsAString& scheme, const char** schemes) {
    for (; *schemes; schemes++) {
      if (scheme.LowerCaseEqualsASCII(*schemes)) {
        return true;
      }
    }
    return false;
  };

  // Formatters

  already_AddRefed<nsIStringBundle> GetStringBundle() {
    nsCOMPtr<nsIStringBundleService> sbs =
        mozilla::components::StringBundle::Service();
    NS_ENSURE_TRUE(sbs, nullptr);

    nsCOMPtr<nsIStringBundle> stringBundle;
    sbs->CreateBundle("chrome://global/locale/extensions.properties",
                      getter_AddRefs(stringBundle));

    return stringBundle.forget();
  };

  void FormatErrorParams(const char* aName, const nsTArray<nsString>& aParams) {
    nsresult rv = NS_ERROR_FAILURE;

    nsCOMPtr<nsIStringBundle> stringBundle = GetStringBundle();

    if (stringBundle) {
      rv = stringBundle->FormatStringFromName(aName, aParams, mError);
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      mError.AssignLiteral("An unexpected error occurred");
    }
  };

  // Data members

  nsAutoString mURL;
  NS_ConvertASCIItoUTF16 mDirective;
  nsString mError;

  uint32_t mPermittedPolicy;
  bool mFoundSelf;
};

/**
 * Validates a custom content security policy string for use by an add-on.
 * In particular, ensures that:
 *
 *  - Both object-src and script-src directives are present, and meet
 *    the policies required by the CSPValidator class
 *
 *  - The script-src directive includes the source 'self'
 */
NS_IMETHODIMP
AddonContentPolicy::ValidateAddonCSP(const nsAString& aPolicyString,
                                     uint32_t aPermittedPolicy,
                                     nsAString& aResult) {
  nsresult rv;

  // Validate against a randomly-generated extension origin.
  // There is no add-on-specific behavior in the CSP code, beyond the ability
  // for add-ons to specify a custom policy, but the parser requires a valid
  // origin in order to operate correctly.
  nsAutoString url(u"moz-extension://");
  {
    nsCOMPtr<nsIUUIDGenerator> uuidgen = components::UUIDGenerator::Service();
    NS_ENSURE_TRUE(uuidgen, NS_ERROR_FAILURE);

    nsID id;
    rv = uuidgen->GenerateUUIDInPlace(&id);
    NS_ENSURE_SUCCESS(rv, rv);

    char idString[NSID_LENGTH];
    id.ToProvidedString(idString);

    MOZ_RELEASE_ASSERT(idString[0] == '{' && idString[NSID_LENGTH - 2] == '}',
                       "UUID generator did not return a valid UUID");

    url.AppendASCII(idString + 1, NSID_LENGTH - 3);
  }

  RefPtr<BasePrincipal> principal =
      BasePrincipal::CreateContentPrincipal(NS_ConvertUTF16toUTF8(url));

  nsCOMPtr<nsIURI> selfURI;
  principal->GetURI(getter_AddRefs(selfURI));
  RefPtr<nsCSPContext> csp = new nsCSPContext();
  rv = csp->SetRequestContextWithPrincipal(principal, selfURI, u""_ns, 0);
  NS_ENSURE_SUCCESS(rv, rv);
  csp->AppendPolicy(aPolicyString, false, false);

  const nsCSPPolicy* policy = csp->GetPolicy(0);
  if (!policy) {
    CSPValidator validator(url, nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE,
                           true, aPermittedPolicy);
    aResult.Assign(validator.GetError());
    return NS_OK;
  }

  bool haveValidDefaultSrc = false;
  bool hasValidScriptSrc = false;
  {
    CSPDirective directive = nsIContentSecurityPolicy::DEFAULT_SRC_DIRECTIVE;
    CSPValidator validator(url, directive);

    haveValidDefaultSrc = policy->visitDirectiveSrcs(directive, &validator);
  }

  aResult.SetIsVoid(true);
  {
    CSPDirective directive = nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE;
    CSPValidator validator(url, directive, !haveValidDefaultSrc,
                           aPermittedPolicy);

    if (!policy->visitDirectiveSrcs(directive, &validator)) {
      aResult.Assign(validator.GetError());
    } else if (!validator.FoundSelf()) {
      validator.FormatError("csp.error.missing-source", u"'self'"_ns);
      aResult.Assign(validator.GetError());
    }
    hasValidScriptSrc = true;
  }

  if (aResult.IsVoid()) {
    CSPDirective directive = nsIContentSecurityPolicy::OBJECT_SRC_DIRECTIVE;
    CSPValidator validator(url, directive, !haveValidDefaultSrc,
                           aPermittedPolicy);

    if (!policy->visitDirectiveSrcs(directive, &validator)) {
      aResult.Assign(validator.GetError());
    }
  }

  if (aResult.IsVoid()) {
    CSPDirective directive = nsIContentSecurityPolicy::WORKER_SRC_DIRECTIVE;
    CSPValidator validator(url, directive,
                           !haveValidDefaultSrc && !hasValidScriptSrc,
                           aPermittedPolicy);

    if (!policy->visitDirectiveSrcs(directive, &validator)) {
      aResult.Assign(validator.GetError());
    }
  }

  return NS_OK;
}

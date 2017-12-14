/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoginReputation.h"
#include "nsIDOMHTMLInputElement.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;

#define PREF_PP_ENABLED    "browser.safebrowsing.passwords.enabled"
static bool sPasswordProtectionEnabled = false;

// MOZ_LOG=LoginReputation:5
LazyLogModule gLoginReputationLogModule("LoginReputation");
#define LR_LOG(args) MOZ_LOG(gLoginReputationLogModule, mozilla::LogLevel::Debug, args)
#define LR_LOG_ENABLED() MOZ_LOG_TEST(gLoginReputationLogModule, mozilla::LogLevel::Debug)

// -------------------------------------------------------------------------
// ReputationQueryParam
//
// Concrete class for nsILoginReputationQuery to hold query parameters
//
class ReputationQueryParam final : public nsILoginReputationQuery
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSILOGINREPUTATIONQUERY

  explicit ReputationQueryParam(nsIURI* aURI)
    : mURI(aURI)
  {
  };

private:
  ~ReputationQueryParam() = default;

  nsCOMPtr<nsIURI> mURI;
};

NS_IMPL_ISUPPORTS(ReputationQueryParam, nsILoginReputationQuery)

NS_IMETHODIMP
ReputationQueryParam::GetFormURI(nsIURI** aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

// -------------------------------------------------------------------------
// LoginReputationService
//
NS_IMPL_ISUPPORTS(LoginReputationService,
                  nsILoginReputationService)

LoginReputationService*
  LoginReputationService::gLoginReputationService = nullptr;

already_AddRefed<LoginReputationService>
LoginReputationService::GetSingleton()
{
  if (!gLoginReputationService) {
    gLoginReputationService = new LoginReputationService();
  }
  return do_AddRef(gLoginReputationService);
}

LoginReputationService::LoginReputationService()
{
  LR_LOG(("Login reputation service starting up"));
}

LoginReputationService::~LoginReputationService()
{
  LR_LOG(("Login reputation service shutting down"));
}

NS_IMETHODIMP
LoginReputationService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  switch (XRE_GetProcessType()) {
  case GeckoProcessType_Default:
    LR_LOG(("Init login reputation service in parent"));
    break;
  case GeckoProcessType_Content:
    LR_LOG(("Init login reputation service in child"));
    break;
  default:
    // No other process type is supported!
    return NS_ERROR_NOT_AVAILABLE;
  }

  Preferences::AddBoolVarCache(&sPasswordProtectionEnabled, PREF_PP_ENABLED, true);

  return NS_OK;
}

// static
already_AddRefed<nsILoginReputationQuery>
LoginReputationService::ConstructQueryParam(nsIURI* aURI)
{
  RefPtr<ReputationQueryParam> param = new ReputationQueryParam(aURI);
  return param.forget();
}

NS_IMETHODIMP
LoginReputationService::QueryReputationAsync(nsIDOMHTMLInputElement* aInput,
                                             nsILoginReputationQueryCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aInput);

  LR_LOG(("QueryReputationAsync() [this=%p]", this));

  if (!sPasswordProtectionEnabled) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(aInput);
  NS_ENSURE_STATE(node);

  nsIURI* documentURI = node->OwnerDoc()->GetDocumentURI();
  NS_ENSURE_STATE(documentURI);

  if (XRE_IsContentProcess()) {
  } else {
    nsCOMPtr<nsILoginReputationQuery> query =
      LoginReputationService::ConstructQueryParam(documentURI);

    nsresult rv = QueryReputation(query, aCallback);
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
LoginReputationService::QueryReputation(nsILoginReputationQuery* aQuery,
                                        nsILoginReputationQueryCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aQuery);

  LR_LOG(("QueryReputation() [this=%p]", this));

  if (!sPasswordProtectionEnabled) {
    return NS_ERROR_FAILURE;
  }

  // Return SAFE until we add support for the remote service (bug 1413732).
  if (aCallback) {
    aCallback->OnQueryComplete(nsILoginReputationResult::SAFE);
  }

  return NS_OK;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LoginReputation_h__
#define LoginReputation_h__

#include "nsILoginReputation.h"
#include "nsIURIClassifier.h"
#include "nsIObserver.h"
#include "mozilla/Logging.h"
#include "mozilla/MozPromise.h"

class LoginWhitelist;

namespace mozilla {

typedef uint32_t VerdictType;
typedef MozPromise<VerdictType, nsresult, false> ReputationPromise;

class LoginReputationService final : public nsILoginReputationService,
                                     public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOGINREPUTATIONSERVICE
  NS_DECL_NSIOBSERVER

public:
  static
  already_AddRefed<LoginReputationService> GetSingleton();

  static
  already_AddRefed<nsILoginReputationQuery> ConstructQueryParam(nsIURI* aURI);

  static
  nsCString VerdictTypeToString(VerdictType aVerdict);

private:

  struct QueryRequest {
    QueryRequest(nsILoginReputationQuery* aParam,
                 nsILoginReputationQueryCallback* aCallback) :
      mParam(aParam),
      mCallback(aCallback)
    {
    }

    nsCOMPtr<nsILoginReputationQuery> mParam;
    nsCOMPtr<nsILoginReputationQueryCallback> mCallback;
  };

  /**
   * Global singleton object for holding this factory service.
   */
  static LoginReputationService* gLoginReputationService;

  LoginReputationService();
  ~LoginReputationService();

  nsresult Enable();

  nsresult Disable();

  nsresult QueryLoginWhitelist(QueryRequest* aRequest);

  // Called when a query request is finished.
  nsresult Finish(const QueryRequest* aRequest,
                  nsresult aStatus,
                  VerdictType aVerdict);

  // Clear data and join the worker threads.
  nsresult Shutdown();

  RefPtr<LoginWhitelist> mLoginWhitelist;

  // Array that holds ongoing query requests which are added when
  // ::QueryReputation is called.
  nsTArray<UniquePtr<QueryRequest>> mQueryRequests;
};

}  // namespace mozilla

#endif  // LoginReputation_h__

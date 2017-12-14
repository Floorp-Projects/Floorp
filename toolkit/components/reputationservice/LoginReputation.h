/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LoginReputation_h__
#define LoginReputation_h__

#include "nsILoginReputation.h"
#include "mozilla/Logging.h"

class LoginReputationService final : public nsILoginReputationService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOGINREPUTATIONSERVICE

public:
  static
  already_AddRefed<LoginReputationService> GetSingleton();

  static
  already_AddRefed<nsILoginReputationQuery> ConstructQueryParam(nsIURI* aURI);

private:
  /**
   * Global singleton object for holding this factory service.
   */
  static LoginReputationService* gLoginReputationService;

  LoginReputationService();
  ~LoginReputationService();
};

#endif  // LoginReputation_h__

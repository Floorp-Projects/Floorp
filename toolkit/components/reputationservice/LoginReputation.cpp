/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoginReputation.h"

using namespace mozilla;

// MOZ_LOG=LoginReputation:5
LazyLogModule LoginReputationService::prlog("LoginReputation");
#define LR_LOG(args) MOZ_LOG(LoginReputationService::prlog, mozilla::LogLevel::Debug, args)
#define LR_LOG_ENABLED() MOZ_LOG_TEST(LoginReputationService::prlog, mozilla::LogLevel::Debug)

NS_IMPL_ISUPPORTS(LoginReputationService,
                  ILoginReputationService)

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

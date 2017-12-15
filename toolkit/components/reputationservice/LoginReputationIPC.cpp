
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoginReputationIPC.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::dom;

// MOZ_LOG=LoginReputation:5
extern LazyLogModule gLoginReputationLogModule;
#define LR_LOG(args) MOZ_LOG(gLoginReputationLogModule, mozilla::LogLevel::Debug, args)
#define LR_LOG_ENABLED() MOZ_LOG_TEST(gLoginReputationLogModule, mozilla::LogLevel::Debug)

NS_IMPL_ISUPPORTS(LoginReputationParent, nsILoginReputationQueryCallback)

mozilla::ipc::IPCResult
LoginReputationParent::QueryReputation(nsIURI* aURI)
{
  nsresult rv;
  nsCOMPtr<nsILoginReputationService> service =
    do_GetService(NS_LOGIN_REPUTATION_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    Unused << Send__delete__(this);
    return IPC_OK();
  }

  nsCOMPtr<nsILoginReputationQuery> query =
    LoginReputationService::ConstructQueryParam(aURI);
  rv = service->QueryReputation(query, this);
  if (NS_FAILED(rv)) {
    Unused << Send__delete__(this);
  }

  return IPC_OK();
}

NS_IMETHODIMP
LoginReputationParent::OnQueryComplete(uint16_t aResult)
{
  LR_LOG(("OnQueryComplete() [result=%d]", aResult));

  if (mIPCOpen) {
    Unused << Send__delete__(this);
  }
  return NS_OK;
}

void
LoginReputationParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mIPCOpen = false;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OSReauthenticator.h"

#include "OSKeyStore.h"

NS_IMPL_ISUPPORTS(OSReauthenticator, nsIOSReauthenticator)

using namespace mozilla;
using dom::Promise;

static nsresult
ReauthenticateUser(const nsACString& prompt, /* out */ bool& reauthenticated)
{
  reauthenticated = false;
  return NS_OK;
}

static void
BackgroundReauthenticateUser(RefPtr<Promise>& aPromise,
                             const nsACString& aPrompt)
{
  nsAutoCString recovery;
  bool reauthenticated;
  nsresult rv = ReauthenticateUser(aPrompt, reauthenticated);
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
    "BackgroundReauthenticateUserResolve",
    [rv, reauthenticated, aPromise = std::move(aPromise)]() {
      if (NS_FAILED(rv)) {
        aPromise->MaybeReject(rv);
      } else {
        aPromise->MaybeResolve(reauthenticated);
      }
    }));
  NS_DispatchToMainThread(runnable.forget());
}

NS_IMETHODIMP
OSReauthenticator::AsyncReauthenticateUser(const nsACString& aPrompt,
                                           JSContext* aCx,
                                           Promise** promiseOut)
{
  NS_ENSURE_ARG_POINTER(aCx);

  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIRunnable> runnable(
    NS_NewRunnableFunction("BackgroundReauthenticateUser",
      [promiseHandle, aPrompt = nsAutoCString(aPrompt)]() mutable {
        BackgroundReauthenticateUser(promiseHandle, aPrompt);
      }
    )
  );

  nsCOMPtr<nsIThread> thread;
  rv = NS_NewNamedThread(NS_LITERAL_CSTRING("ReauthenticateUserThread"),
                         getter_AddRefs(thread), runnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promiseHandle.forget(promiseOut);
  return NS_OK;
}

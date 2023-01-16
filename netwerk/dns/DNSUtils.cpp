/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DNSUtils.h"

#include "nsContentUtils.h"
#include "nsHttpHandler.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIOService.h"
#include "mozilla/SyncRunnable.h"
#include "TRRServiceChannel.h"
#include "TRRLoadInfo.h"

namespace mozilla {
namespace net {

static void InitHttpHandler() {
  nsresult rv;
  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIProtocolHandler> handler;
  rv = ios->GetProtocolHandler("http", getter_AddRefs(handler));
  if (NS_FAILED(rv)) {
    return;
  }
}

// static
nsresult DNSUtils::CreateChannelHelper(nsIURI* aUri, nsIChannel** aResult) {
  *aResult = nullptr;

  if (NS_IsMainThread() && !XRE_IsSocketProcess()) {
    nsresult rv;
    nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_NewChannel(
        aResult, aUri, nsContentUtils::GetSystemPrincipal(),
        nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
        nsIContentPolicy::TYPE_OTHER,
        nullptr,  // nsICookieJarSettings
        nullptr,  // PerformanceStorage
        nullptr,  // aLoadGroup
        nullptr,  // aCallbacks
        nsIRequest::LOAD_NORMAL, ios);
  }

  // Unfortunately, we can only initialize gHttpHandler on main thread.
  if (!gHttpHandler) {
    nsCOMPtr<nsIEventTarget> main = GetMainThreadSerialEventTarget();
    if (main) {
      // Forward to the main thread synchronously.
      SyncRunnable::DispatchToThread(
          main, NS_NewRunnableFunction("InitHttpHandler",
                                       []() { InitHttpHandler(); }));
    }
  }

  if (!gHttpHandler) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<TRRLoadInfo> loadInfo =
      new TRRLoadInfo(aUri, nsIContentPolicy::TYPE_OTHER);
  return gHttpHandler->CreateTRRServiceChannel(aUri,
                                               nullptr,   // givenProxyInfo
                                               0,         // proxyResolveFlags
                                               nullptr,   // proxyURI
                                               loadInfo,  // aLoadInfo
                                               aResult);
}

}  // namespace net
}  // namespace mozilla

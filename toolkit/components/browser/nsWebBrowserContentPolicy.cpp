/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWebBrowserContentPolicy.h"
#include "nsIDocShell.h"
#include "nsCOMPtr.h"
#include "nsContentPolicyUtils.h"
#include "nsNetUtil.h"

nsWebBrowserContentPolicy::nsWebBrowserContentPolicy() = default;

nsWebBrowserContentPolicy::~nsWebBrowserContentPolicy() = default;

NS_IMPL_ISUPPORTS(nsWebBrowserContentPolicy, nsIContentPolicy)

NS_IMETHODIMP
nsWebBrowserContentPolicy::ShouldLoad(nsIURI* aContentLocation,
                                      nsILoadInfo* aLoadInfo,
                                      int16_t* aShouldLoad) {
  MOZ_ASSERT(aShouldLoad, "Null out param");

  ExtContentPolicyType contentType = aLoadInfo->GetExternalContentPolicyType();

  *aShouldLoad = nsIContentPolicy::ACCEPT;

  nsCOMPtr<nsISupports> context = aLoadInfo->GetLoadingContext();
  nsIDocShell* shell = NS_CP_GetDocShellFromContext(context);
  /* We're going to dereference shell, so make sure it isn't null */
  if (!shell) {
    return NS_OK;
  }

  nsresult rv;
  bool allowed = true;

  switch (contentType) {
    case ExtContentPolicy::TYPE_SUBDOCUMENT:
      rv = shell->GetAllowSubframes(&allowed);
      break;
    case ExtContentPolicy::TYPE_IMAGE:
    case ExtContentPolicy::TYPE_IMAGESET:
      rv = shell->GetAllowImages(&allowed);
      break;
    default:
      return NS_OK;
  }

  if (NS_SUCCEEDED(rv) && !allowed) {
    NS_SetRequestBlockingReason(
        aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_WEB_BROWSER);
    *aShouldLoad = nsIContentPolicy::REJECT_TYPE;
  }
  return rv;
}

NS_IMETHODIMP
nsWebBrowserContentPolicy::ShouldProcess(nsIURI* aContentLocation,
                                         nsILoadInfo* aLoadInfo,
                                         int16_t* aShouldProcess) {
  MOZ_ASSERT(aShouldProcess, "Null out param");

  ExtContentPolicyType contentType = aLoadInfo->GetExternalContentPolicyType();

  *aShouldProcess = nsIContentPolicy::ACCEPT;

  // Object tags will always open channels with TYPE_OBJECT, but may end up
  // loading with TYPE_IMAGE or TYPE_DOCUMENT as their final type, so we block
  // actual-plugins at the process stage
  if (contentType != ExtContentPolicy::TYPE_OBJECT) {
    return NS_OK;
  }

  nsCOMPtr<nsISupports> context = aLoadInfo->GetLoadingContext();
  nsIDocShell* shell = NS_CP_GetDocShellFromContext(context);
  if (shell && (!shell->PluginsAllowedInCurrentDoc())) {
    NS_SetRequestBlockingReason(
        aLoadInfo, nsILoadInfo::BLOCKING_REASON_CONTENT_POLICY_WEB_BROWSER);
    *aShouldProcess = nsIContentPolicy::REJECT_TYPE;
  }

  return NS_OK;
}

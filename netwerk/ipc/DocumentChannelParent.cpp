/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentChannelParent.h"

namespace mozilla {
namespace net {

bool DocumentChannelParent::Init(const DocumentChannelCreationArgs& aArgs) {
  RefPtr<nsDocShellLoadState> loadState =
      new nsDocShellLoadState(aArgs.loadState());

  RefPtr<class LoadInfo> loadInfo;
  nsresult rv = mozilla::ipc::LoadInfoArgsToLoadInfo(Some(aArgs.loadInfo()),
                                                     getter_AddRefs(loadInfo));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = NS_ERROR_UNEXPECTED;
  if (!mParent->Open(loadState, loadInfo, aArgs.initiatorType().ptrOr(nullptr),
                     aArgs.loadFlags(), aArgs.loadType(), aArgs.cacheKey(),
                     aArgs.isActive(), aArgs.isTopLevelDoc(),
                     aArgs.hasNonEmptySandboxingFlags(), aArgs.topWindowURI(),
                     aArgs.contentBlockingAllowListPrincipal(),
                     aArgs.customUserAgent(), aArgs.channelId(),
                     aArgs.asyncOpenTime(), &rv)) {
    return SendFailedAsyncOpen(rv);
  }

  return true;
}

RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
DocumentChannelParent::RedirectToRealChannel(uint32_t aRedirectFlags,
                                             uint32_t aLoadFlags) {
  RedirectToRealChannelArgs args;
  mParent->SerializeRedirectData(args, false, aRedirectFlags, aLoadFlags);
  return SendRedirectToRealChannel(args);
}

}  // namespace net
}  // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentChannelParent.h"

extern mozilla::LazyLogModule gDocumentChannelLog;
#define LOG(fmt) MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Verbose, fmt)

namespace mozilla {
namespace net {

DocumentChannelParent::DocumentChannelParent(BrowserParent* aBrowser,
                                             nsILoadContext* aLoadContext,
                                             PBOverrideStatus aOverrideStatus) {
  LOG(("DocumentChannelParent ctor [this=%p]", this));
  mParent =
      new DocumentLoadListener(aBrowser, aLoadContext, aOverrideStatus, this);
}

DocumentChannelParent::~DocumentChannelParent() {
  LOG(("DocumentChannelParent dtor [this=%p]", this));
}

bool DocumentChannelParent::Init(BrowserParent* aBrowser,
                                 const DocumentChannelCreationArgs& aArgs) {
  RefPtr<nsDocShellLoadState> loadState =
      new nsDocShellLoadState(aArgs.loadState());
  LOG(("DocumentChannelParent Init [this=%p, uri=%s]", this,
       loadState->URI()->GetSpecOrDefault().get()));

  RefPtr<class LoadInfo> loadInfo;
  nsresult rv = mozilla::ipc::LoadInfoArgsToLoadInfo(Some(aArgs.loadInfo()),
                                                     getter_AddRefs(loadInfo));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = NS_ERROR_UNEXPECTED;
  if (!mParent->Open(
          aBrowser, loadState, loadInfo, aArgs.initiatorType().ptrOr(nullptr),
          aArgs.loadFlags(), aArgs.loadType(), aArgs.cacheKey(),
          aArgs.isActive(), aArgs.isTopLevelDoc(),
          aArgs.hasNonEmptySandboxingFlags(), aArgs.topWindowURI(),
          aArgs.contentBlockingAllowListPrincipal(), aArgs.customUserAgent(),
          aArgs.channelId(), aArgs.asyncOpenTime(), aArgs.documentOpenFlags(),
          aArgs.pluginsAllowed(), &rv)) {
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

#undef LOG

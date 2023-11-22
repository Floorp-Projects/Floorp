/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __JumpListBuilder_h__
#define __JumpListBuilder_h__

#include <windows.h>

// Needed for various com interfaces
#include <shobjidl.h>
#undef LogSeverity  // SetupAPI.h #defines this as DWORD

#include "nsString.h"

#include "nsIJumpListBuilder.h"
#include "nsIJumpListItem.h"
#include "JumpListItem.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/mscom/AgileReference.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {
namespace widget {

namespace detail {
class DoneCommitListBuildCallback;
}  // namespace detail

class JumpListBuilder : public nsIJumpListBuilder, public nsIObserver {
  virtual ~JumpListBuilder();

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIJUMPLISTBUILDER
  NS_DECL_NSIOBSERVER

  JumpListBuilder();

 protected:
  static Atomic<bool> sBuildingList;

 private:
  mscom::AgileReference mJumpListMgr MOZ_GUARDED_BY(mMonitor);
  uint32_t mMaxItems MOZ_GUARDED_BY(mMonitor);
  bool mHasCommit;
  RefPtr<LazyIdleThread> mIOThread;
  ReentrantMonitor mMonitor;
  nsString mAppUserModelId;

  bool IsSeparator(nsCOMPtr<nsIJumpListItem>& item);
  void RemoveIconCacheAndGetJumplistShortcutURIs(IObjectArray* aObjArray,
                                                 nsTArray<nsString>& aURISpecs);
  void DeleteIconFromDisk(const nsAString& aPath);
  nsresult RemoveIconCacheForAllItems();
  void DoCommitListBuild(RefPtr<detail::DoneCommitListBuildCallback> aCallback);
  void DoInitListBuild(RefPtr<dom::Promise>&& aPromise);

  friend class WinTaskbar;
};

}  // namespace widget
}  // namespace mozilla

#endif /* __JumpListBuilder_h__ */

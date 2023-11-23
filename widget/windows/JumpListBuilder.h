/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __JumpListBuilder_h__
#define __JumpListBuilder_h__

#include "nsIJumpListBuilder.h"

#include "nsIObserver.h"
#include "nsProxyRelease.h"
#include "mozilla/LazyIdleThread.h"

namespace mozilla {

namespace dom {
struct WindowsJumpListShortcutDescription;
}  // namespace dom

namespace widget {

/**
 * This is an abstract class for a backend to write to the Windows Jump List.
 *
 * It has a 1-to-1 method mapping with ICustomDestinationList. The abtract
 * class allows us to implement a "fake" backend for automated testing.
 */
class JumpListBackend {
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual bool IsAvailable() = 0;

  virtual HRESULT SetAppID(LPCWSTR pszAppID) = 0;
  virtual HRESULT BeginList(UINT* pcMinSlots, REFIID riid, void** ppv) = 0;
  virtual HRESULT AddUserTasks(IObjectArray* poa) = 0;
  virtual HRESULT AppendCategory(LPCWSTR pszCategory, IObjectArray* poa) = 0;
  virtual HRESULT CommitList() = 0;
  virtual HRESULT AbortList() = 0;
  virtual HRESULT DeleteList(LPCWSTR pszAppID) = 0;
  virtual HRESULT AppendKnownCategory(KNOWNDESTCATEGORY category) = 0;

 protected:
  virtual ~JumpListBackend() {}
};

/**
 * JumpListBuilder is a component that can be used to manage the Windows
 * Jump List off of the main thread.
 */
class JumpListBuilder : public nsIJumpListBuilder, public nsIObserver {
  virtual ~JumpListBuilder();

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIJUMPLISTBUILDER
  NS_DECL_NSIOBSERVER

  explicit JumpListBuilder(const nsAString& aAppUserModelId,
                           RefPtr<JumpListBackend> aTestingBackend = nullptr);

 private:
  // These all run on the lazy helper thread.
  void DoSetupBackend();
  void DoSetupTestingBackend(RefPtr<JumpListBackend> aTestingBackend);
  void DoShutdownBackend();
  void DoSetAppID(nsString aAppUserModelID);
  void DoIsAvailable(const nsMainThreadPtrHandle<dom::Promise>& aPromiseHolder);
  void DoCheckForRemovals(
      const nsMainThreadPtrHandle<dom::Promise>& aPromiseHolder);
  void DoPopulateJumpList(
      const nsTArray<dom::WindowsJumpListShortcutDescription>&&
          aTaskDescriptions,
      const nsAString& aCustomTitle,
      const nsTArray<dom::WindowsJumpListShortcutDescription>&&
          aCustomDescriptions,
      const nsMainThreadPtrHandle<dom::Promise>& aPromiseHolder);
  void DoClearJumpList(
      const nsMainThreadPtrHandle<dom::Promise>& aPromiseHolder);
  void RemoveIconCacheAndGetJumplistShortcutURIs(IObjectArray* aObjArray,
                                                 nsTArray<nsString>& aURISpecs);
  void DeleteIconFromDisk(const nsAString& aPath);
  nsresult GetShellLinkFromDescription(
      const dom::WindowsJumpListShortcutDescription& aDesc,
      RefPtr<IShellLinkW>& aShellLink);

  // This is written to once during construction on the main thread before the
  // lazy helper thread is created. After that, the lazy helper thread might
  // read from it.
  nsString mAppUserModelId;

  // This is only accessed by the lazy helper thread.
  RefPtr<JumpListBackend> mJumpListBackend;

  // This is only accessed by the main thread.
  RefPtr<LazyIdleThread> mIOThread;
};

}  // namespace widget
}  // namespace mozilla

#endif /* __JumpListBuilder_h__ */

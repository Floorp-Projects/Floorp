/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWindowWatcher_h__
#define __nsWindowWatcher_h__

// {a21bfa01-f349-4394-a84c-8de5cf0737d0}
#define NS_WINDOWWATCHER_CID                        \
  {                                                 \
    0xa21bfa01, 0xf349, 0x4394, {                   \
      0xa8, 0x4c, 0x8d, 0xe5, 0xcf, 0x7, 0x37, 0xd0 \
    }                                               \
  }

#include "nsCOMPtr.h"
#include "mozilla/Mutex.h"
#include "mozilla/Maybe.h"
#include "nsIWindowCreator.h"  // for stupid compilers
#include "nsIWindowWatcher.h"
#include "nsIOpenWindowInfo.h"
#include "nsIPromptFactory.h"
#include "nsIRemoteTab.h"
#include "nsPIWindowWatcher.h"
#include "nsTArray.h"
#include "mozilla/dom/WindowFeatures.h"  // mozilla::dom::WindowFeatures

class nsIURI;
class nsIDocShellTreeItem;
class nsIDocShellTreeOwner;
class nsPIDOMWindowOuter;
class nsWatcherWindowEnumerator;
class nsPromptService;
struct nsWatcherWindowEntry;
struct SizeSpec;

class nsWindowWatcher : public nsIWindowWatcher,
                        public nsPIWindowWatcher,
                        public nsIPromptFactory {
  friend class nsWatcherWindowEnumerator;

 public:
  nsWindowWatcher();

  nsresult Init();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIWINDOWWATCHER
  NS_DECL_NSPIWINDOWWATCHER
  NS_DECL_NSIPROMPTFACTORY

  static int32_t GetWindowOpenLocation(nsPIDOMWindowOuter* aParent,
                                       uint32_t aChromeFlags,
                                       bool aCalledFromJS,
                                       bool aWidthSpecified);

  // Will first look for a caller on the JS stack, and then fall back on
  // aCurrentContext if it can't find one.
  // It also knows to not look for things if aForceNoOpener is set.
  already_AddRefed<mozilla::dom::BrowsingContext> GetBrowsingContextByName(
      const nsAString& aName, bool aForceNoOpener,
      mozilla::dom::BrowsingContext* aCurrentContext);

 protected:
  virtual ~nsWindowWatcher();

  friend class nsPromptService;
  bool AddEnumerator(nsWatcherWindowEnumerator* aEnumerator);
  bool RemoveEnumerator(nsWatcherWindowEnumerator* aEnumerator);

  nsWatcherWindowEntry* FindWindowEntry(mozIDOMWindowProxy* aWindow);
  nsresult RemoveWindow(nsWatcherWindowEntry* aInfo);

  // Just like OpenWindowJS, but knows whether it got called via OpenWindowJS
  // (which means called from script) or called via OpenWindow.
  nsresult OpenWindowInternal(mozIDOMWindowProxy* aParent,
                              const nsACString& aUrl, const nsACString& aName,
                              const nsACString& aFeatures, bool aCalledFromJS,
                              bool aDialog, bool aNavigate, nsIArray* aArgv,
                              bool aIsPopupSpam, bool aForceNoOpener,
                              bool aForceNoReferrer,
                              nsDocShellLoadState* aLoadState,
                              mozilla::dom::BrowsingContext** aResult);

  static nsresult URIfromURL(const nsACString& aURL,
                             mozIDOMWindowProxy* aParent, nsIURI** aURI);

  static bool ShouldOpenPopup(const mozilla::dom::WindowFeatures& aFeatures,
                              const SizeSpec& aSizeSpec);

  static uint32_t CalculateChromeFlagsForContent(
      const mozilla::dom::WindowFeatures& aFeatures, const SizeSpec& aSizeSpec);

  static uint32_t CalculateChromeFlagsForSystem(
      const mozilla::dom::WindowFeatures& aFeatures, const SizeSpec& aSizeSpec,
      bool aDialog, bool aChromeURL, bool aHasChromeParent);

  /* Compute the right SizeSpec based on aFeatures */
  static void CalcSizeSpec(const mozilla::dom::WindowFeatures& aFeatures,
                           bool aHasChromeParent, SizeSpec& aResult);
  static void SizeOpenedWindow(
      nsIDocShellTreeOwner* aTreeOwner, mozIDOMWindowProxy* aParent,
      bool aIsCallerChrome, const SizeSpec& aSizeSpec,
      const mozilla::Maybe<float>& aOpenerFullZoom = mozilla::Nothing());

 private:
  nsresult CreateChromeWindow(nsIWebBrowserChrome* aParentChrome,
                              uint32_t aChromeFlags,
                              nsIOpenWindowInfo* aOpenWindowInfo,
                              nsIWebBrowserChrome** aResult);

  void MaybeDisablePersistence(const SizeSpec& sizeSpec,
                               nsIDocShellTreeOwner* aTreeOwner);

  static uint32_t CalculateChromeFlagsHelper(
      uint32_t aInitialFlags, const mozilla::dom::WindowFeatures& aFeatures,
      const SizeSpec& aSizeSpec, bool* presenceFlag = nullptr,
      bool aHasChromeParent = false);
  static uint32_t EnsureFlagsSafeForContent(uint32_t aChromeFlags,
                                            bool aChromeURL = false);

 protected:
  nsTArray<nsWatcherWindowEnumerator*> mEnumeratorList;
  nsWatcherWindowEntry* mOldestWindow;
  mozilla::Mutex mListLock;

  nsCOMPtr<nsIWindowCreator> mWindowCreator;
};

#endif

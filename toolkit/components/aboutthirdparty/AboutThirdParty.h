/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AboutThirdParty_h__
#define __AboutThirdParty_h__

#include "mozilla/MozPromise.h"
#include "nsIAboutThirdParty.h"
#include "nsInterfaceHashtable.h"
#include "nsTArray.h"
#include "nsTHashMap.h"
#include "nsTHashSet.h"

namespace mozilla {

class DynamicBlocklistWriter;

using InstallLocationT =
    CompactPair<nsString, nsCOMPtr<nsIInstalledApplication>>;
using ComponentPathMapT = nsInterfaceHashtable<nsStringCaseInsensitiveHashKey,
                                               nsIInstalledApplication>;

enum class KnownModuleType : uint32_t {
  Ime = 0,
  IconOverlay,
  ContextMenuHandler,
  CopyHookHandler,
  DragDropHandler,
  PropertySheetHandler,
  DataHandler,
  DropHandler,
  IconHandler,
  InfotipHandler,
  PropertyHandler,

  Last,
};

struct InstallLocationComparator {
  const nsAString& mFilePath;

  explicit InstallLocationComparator(const nsAString& aFilePath);
  int operator()(const InstallLocationT& aLocation) const;
};

class InstalledApplication final : public nsIInstalledApplication {
  nsString mName;
  nsString mPublisher;

  ~InstalledApplication() = default;

 public:
  InstalledApplication() = default;
  InstalledApplication(nsString&& aAppName, nsString&& aPublisher);

  InstalledApplication(InstalledApplication&&) = delete;
  InstalledApplication& operator=(InstalledApplication&&) = delete;
  InstalledApplication(const InstalledApplication&) = delete;
  InstalledApplication& operator=(const InstalledApplication&) = delete;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINSTALLEDAPPLICATION
};

using BackgroundThreadPromise =
    MozPromise<bool /* aIgnored */, nsresult, /* IsExclusive */ false>;

class AboutThirdParty final : public nsIAboutThirdParty {
  // Atomic only supports 32-bit or 64-bit types.
  enum class WorkerState : uint32_t {
    Init,
    Running,
    Done,
  };
  Atomic<WorkerState, SequentiallyConsistent> mWorkerState;
  RefPtr<BackgroundThreadPromise::Private> mPromise;
  nsTHashMap<nsStringCaseInsensitiveHashKey, uint32_t> mKnownModules;
  ComponentPathMapT mComponentPaths;
  nsTArray<InstallLocationT> mLocations;

#if defined(MOZ_LAUNCHER_PROCESS)
  Atomic<DynamicBlocklistWriter*> mPendingWriter;
  // The current blocklist. May differ from mDynamicBlocklistAtLaunch
  // if the user has blocked/unblocked modules. Note that this does not
  // take effect until restart.
  nsTHashSet<nsStringCaseInsensitiveHashKey> mDynamicBlocklist;
  // The blocklist that was used at launch, which is currently in effect.
  nsTHashSet<nsStringCaseInsensitiveHashKey> mDynamicBlocklistAtLaunch;
#endif

  ~AboutThirdParty() = default;
  void BackgroundThread();
  void AddKnownModule(const nsString& aPath, KnownModuleType aType);

 public:
  static already_AddRefed<AboutThirdParty> GetSingleton();

  AboutThirdParty();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIABOUTTHIRDPARTY

  // Have a function separated from dom::Promise so that
  // both JS method and GTest can use.
  RefPtr<BackgroundThreadPromise> CollectSystemInfoAsync();
};

}  // namespace mozilla

#endif  // __AboutThirdParty_h__

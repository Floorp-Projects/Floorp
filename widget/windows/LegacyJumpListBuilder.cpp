/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LegacyJumpListBuilder.h"

#include "nsError.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsArrayUtils.h"
#include "nsWidgetsCID.h"
#include "WinTaskbar.h"
#include "nsDirectoryServiceUtils.h"
#include "mozilla/Preferences.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "mozilla/LazyIdleThread.h"
#include "nsIObserverService.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/mscom/ApartmentRegion.h"
#include "mozilla/mscom/EnsureMTA.h"

#include <shellapi.h>
#include "WinUtils.h"

using mozilla::dom::Promise;

// The amount of time, in milliseconds, that our IO thread will stay alive after
// the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

namespace mozilla {
namespace widget {

// defined in WinTaskbar.cpp
extern const wchar_t* gMozillaJumpListIDGeneric;

Atomic<bool> LegacyJumpListBuilder::sBuildingList(false);
const char kPrefTaskbarEnabled[] = "browser.taskbar.lists.enabled";

NS_IMPL_ISUPPORTS(LegacyJumpListBuilder, nsILegacyJumpListBuilder, nsIObserver)
#define TOPIC_PROFILE_BEFORE_CHANGE "profile-before-change"
#define TOPIC_CLEAR_PRIVATE_DATA "clear-private-data"

namespace detail {

class DoneCommitListBuildCallback final : public nsIRunnable {
  NS_DECL_THREADSAFE_ISUPPORTS

 public:
  DoneCommitListBuildCallback(nsILegacyJumpListCommittedCallback* aCallback,
                              LegacyJumpListBuilder* aBuilder)
      : mCallback(aCallback), mBuilder(aBuilder), mResult(false) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    if (mCallback) {
      Unused << mCallback->Done(mResult);
    }
    // Ensure we are releasing on the main thread.
    Destroy();
    return NS_OK;
  }

  void SetResult(bool aResult) { mResult = aResult; }

 private:
  ~DoneCommitListBuildCallback() {
    // Destructor does not always call on the main thread.
    MOZ_ASSERT(!mCallback);
    MOZ_ASSERT(!mBuilder);
  }

  void Destroy() {
    MOZ_ASSERT(NS_IsMainThread());
    mCallback = nullptr;
    mBuilder = nullptr;
  }

  // These two references MUST be released on the main thread.
  RefPtr<nsILegacyJumpListCommittedCallback> mCallback;
  RefPtr<LegacyJumpListBuilder> mBuilder;
  bool mResult;
};

NS_IMPL_ISUPPORTS(DoneCommitListBuildCallback, nsIRunnable);

}  // namespace detail

LegacyJumpListBuilder::LegacyJumpListBuilder()
    : mMaxItems(0),
      mHasCommit(false),
      mMonitor("LegacyJumpListBuilderMonitor") {
  MOZ_ASSERT(NS_IsMainThread());

  // Instantiate mJumpListMgr in the multithreaded apartment so that proxied
  // calls on that object do not need to interact with the main thread's message
  // pump.
  mscom::EnsureMTA([&]() {
    RefPtr<ICustomDestinationList> jumpListMgr;
    HRESULT hr = ::CoCreateInstance(
        CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER,
        IID_ICustomDestinationList, getter_AddRefs(jumpListMgr));
    if (FAILED(hr)) {
      return;
    }

    ReentrantMonitorAutoEnter lock(mMonitor);
    // Since we are accessing mJumpListMgr across different threads
    // (ie, different apartments), mJumpListMgr must be an agile reference.
    mJumpListMgr = mscom::AgileReference(jumpListMgr);
  });

  if (!mJumpListMgr) {
    return;
  }

  // Make a lazy thread for any IO
  mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS, "Jump List",
                                 LazyIdleThread::ManualShutdown);
  Preferences::AddStrongObserver(this, kPrefTaskbarEnabled);

  nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    observerService->AddObserver(this, TOPIC_PROFILE_BEFORE_CHANGE, false);
    observerService->AddObserver(this, TOPIC_CLEAR_PRIVATE_DATA, false);
  }
}

LegacyJumpListBuilder::~LegacyJumpListBuilder() {
  Preferences::RemoveObserver(this, kPrefTaskbarEnabled);
}

NS_IMETHODIMP LegacyJumpListBuilder::SetAppUserModelID(
    const nsAString& aAppUserModelId) {
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr) return NS_ERROR_NOT_AVAILABLE;

  RefPtr<ICustomDestinationList> jumpListMgr = mJumpListMgr.Resolve();
  if (!jumpListMgr) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mAppUserModelId.Assign(aAppUserModelId);
  // MSIX packages explicitly do not support setting the appid from within
  // the app, as it is set in the package manifest instead.
  if (!mozilla::widget::WinUtils::HasPackageIdentity()) {
    jumpListMgr->SetAppID(mAppUserModelId.get());
  }

  return NS_OK;
}

NS_IMETHODIMP LegacyJumpListBuilder::GetAvailable(int16_t* aAvailable) {
  *aAvailable = false;

  ReentrantMonitorAutoEnter lock(mMonitor);
  if (mJumpListMgr) *aAvailable = true;

  return NS_OK;
}

NS_IMETHODIMP LegacyJumpListBuilder::GetIsListCommitted(bool* aCommit) {
  *aCommit = mHasCommit;

  return NS_OK;
}

NS_IMETHODIMP LegacyJumpListBuilder::GetMaxListItems(int16_t* aMaxItems) {
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr) return NS_ERROR_NOT_AVAILABLE;

  *aMaxItems = 0;

  if (sBuildingList) {
    *aMaxItems = mMaxItems;
    return NS_OK;
  }

  RefPtr<ICustomDestinationList> jumpListMgr = mJumpListMgr.Resolve();
  if (!jumpListMgr) {
    return NS_ERROR_UNEXPECTED;
  }

  IObjectArray* objArray;
  if (SUCCEEDED(jumpListMgr->BeginList(&mMaxItems, IID_PPV_ARGS(&objArray)))) {
    *aMaxItems = mMaxItems;

    if (objArray) objArray->Release();

    jumpListMgr->AbortList();
  }

  return NS_OK;
}

NS_IMETHODIMP LegacyJumpListBuilder::InitListBuild(JSContext* aCx,
                                                   Promise** aPromise) {
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod<StoreCopyPassByRRef<RefPtr<Promise>>>(
          "InitListBuild", this, &LegacyJumpListBuilder::DoInitListBuild,
          promise);
  nsresult rv = mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);
  return NS_OK;
}

void LegacyJumpListBuilder::DoInitListBuild(RefPtr<Promise>&& aPromise) {
  // Since we're invoking COM interfaces to talk to the shell on a background
  // thread, we need to be running inside a multithreaded apartment.
  mscom::MTARegion mta;
  MOZ_ASSERT(mta.IsValid());

  ReentrantMonitorAutoEnter lock(mMonitor);
  MOZ_ASSERT(mJumpListMgr);

  if (sBuildingList) {
    AbortListBuild();
  }

  HRESULT hr = E_UNEXPECTED;
  auto errorHandler = MakeScopeExit([&aPromise, &hr]() {
    if (SUCCEEDED(hr)) {
      return;
    }

    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "InitListBuildReject", [promise = std::move(aPromise)]() {
          promise->MaybeReject(NS_ERROR_FAILURE);
        }));
  });

  RefPtr<ICustomDestinationList> jumpListMgr = mJumpListMgr.Resolve();
  if (!jumpListMgr) {
    return;
  }

  nsTArray<nsString> urisToRemove;
  RefPtr<IObjectArray> objArray;
  hr = jumpListMgr->BeginList(
      &mMaxItems,
      IID_PPV_ARGS(static_cast<IObjectArray**>(getter_AddRefs(objArray))));
  if (FAILED(hr)) {
    return;
  }

  // The returned objArray of removed items are for manually removed items.
  // This does not return items which are removed because they were previously
  // part of the jump list but are no longer part of the jump list.
  sBuildingList = true;
  RemoveIconCacheAndGetJumplistShortcutURIs(objArray, urisToRemove);

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "InitListBuildResolve", [urisToRemove = std::move(urisToRemove),
                               promise = std::move(aPromise)]() {
        promise->MaybeResolve(urisToRemove);
      }));
}

// Ensures that we have no old ICO files left in the jump list cache
nsresult LegacyJumpListBuilder::RemoveIconCacheForAllItems() {
  // Construct the path of our jump list cache
  nsCOMPtr<nsIFile> jumpListCacheDir;
  nsresult rv =
      NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(jumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = jumpListCacheDir->AppendNative(
      nsDependentCString(mozilla::widget::FaviconHelper::kJumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = jumpListCacheDir->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  // Loop through each directory entry and remove all ICO files found
  do {
    nsCOMPtr<nsIFile> currFile;
    if (NS_FAILED(entries->GetNextFile(getter_AddRefs(currFile))) || !currFile)
      break;

    nsAutoString path;
    if (NS_FAILED(currFile->GetPath(path))) continue;

    if (StringTail(path, 4).LowerCaseEqualsASCII(".ico")) {
      // Check if the cached ICO file exists
      bool exists;
      if (NS_FAILED(currFile->Exists(&exists)) || !exists) continue;

      // We found an ICO file that exists, so we should remove it
      currFile->Remove(false);
    }
  } while (true);

  return NS_OK;
}

NS_IMETHODIMP LegacyJumpListBuilder::AddListToBuild(int16_t aCatType,
                                                    nsIArray* items,
                                                    const nsAString& catName,
                                                    bool* _retval) {
  nsresult rv;

  *_retval = false;

  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr) return NS_ERROR_NOT_AVAILABLE;

  RefPtr<ICustomDestinationList> jumpListMgr = mJumpListMgr.Resolve();
  if (!jumpListMgr) {
    return NS_ERROR_UNEXPECTED;
  }

  switch (aCatType) {
    case nsILegacyJumpListBuilder::JUMPLIST_CATEGORY_TASKS: {
      NS_ENSURE_ARG_POINTER(items);

      HRESULT hr;
      RefPtr<IObjectCollection> collection;
      hr = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                            CLSCTX_INPROC_SERVER, IID_IObjectCollection,
                            getter_AddRefs(collection));
      if (FAILED(hr)) return NS_ERROR_UNEXPECTED;

      // Build the list
      uint32_t length;
      items->GetLength(&length);
      for (uint32_t i = 0; i < length; ++i) {
        nsCOMPtr<nsILegacyJumpListItem> item = do_QueryElementAt(items, i);
        if (!item) continue;
        // Check for separators
        if (IsSeparator(item)) {
          RefPtr<IShellLinkW> link;
          rv = LegacyJumpListSeparator::GetSeparator(link);
          if (NS_FAILED(rv)) return rv;
          collection->AddObject(link);
          continue;
        }
        // These should all be ShellLinks
        RefPtr<IShellLinkW> link;
        rv = LegacyJumpListShortcut::GetShellLink(item, link, mIOThread);
        if (NS_FAILED(rv)) return rv;
        collection->AddObject(link);
      }

      // We need IObjectArray to submit
      RefPtr<IObjectArray> pArray;
      hr = collection->QueryInterface(IID_IObjectArray, getter_AddRefs(pArray));
      if (FAILED(hr)) return NS_ERROR_UNEXPECTED;

      // Add the tasks
      hr = jumpListMgr->AddUserTasks(pArray);
      if (SUCCEEDED(hr)) *_retval = true;
      return NS_OK;
    } break;
    case nsILegacyJumpListBuilder::JUMPLIST_CATEGORY_RECENT: {
      if (SUCCEEDED(jumpListMgr->AppendKnownCategory(KDC_RECENT)))
        *_retval = true;
      return NS_OK;
    } break;
    case nsILegacyJumpListBuilder::JUMPLIST_CATEGORY_FREQUENT: {
      if (SUCCEEDED(jumpListMgr->AppendKnownCategory(KDC_FREQUENT)))
        *_retval = true;
      return NS_OK;
    } break;
    case nsILegacyJumpListBuilder::JUMPLIST_CATEGORY_CUSTOMLIST: {
      NS_ENSURE_ARG_POINTER(items);

      if (catName.IsEmpty()) return NS_ERROR_INVALID_ARG;

      HRESULT hr;
      RefPtr<IObjectCollection> collection;
      hr = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                            CLSCTX_INPROC_SERVER, IID_IObjectCollection,
                            getter_AddRefs(collection));
      if (FAILED(hr)) return NS_ERROR_UNEXPECTED;

      uint32_t length;
      items->GetLength(&length);
      for (uint32_t i = 0; i < length; ++i) {
        nsCOMPtr<nsILegacyJumpListItem> item = do_QueryElementAt(items, i);
        if (!item) continue;
        int16_t type;
        if (NS_FAILED(item->GetType(&type))) continue;
        switch (type) {
          case nsILegacyJumpListItem::JUMPLIST_ITEM_SEPARATOR: {
            RefPtr<IShellLinkW> shellItem;
            rv = LegacyJumpListSeparator::GetSeparator(shellItem);
            if (NS_FAILED(rv)) return rv;
            collection->AddObject(shellItem);
          } break;
          case nsILegacyJumpListItem::JUMPLIST_ITEM_LINK: {
            RefPtr<IShellItem2> shellItem;
            rv = LegacyJumpListLink::GetShellItem(item, shellItem);
            if (NS_FAILED(rv)) return rv;
            collection->AddObject(shellItem);
          } break;
          case nsILegacyJumpListItem::JUMPLIST_ITEM_SHORTCUT: {
            RefPtr<IShellLinkW> shellItem;
            rv = LegacyJumpListShortcut::GetShellLink(item, shellItem,
                                                      mIOThread);
            if (NS_FAILED(rv)) return rv;
            collection->AddObject(shellItem);
          } break;
        }
      }

      // We need IObjectArray to submit
      RefPtr<IObjectArray> pArray;
      hr = collection->QueryInterface(IID_IObjectArray, (LPVOID*)&pArray);
      if (FAILED(hr)) return NS_ERROR_UNEXPECTED;

      // Add the tasks
      hr = jumpListMgr->AppendCategory(
          reinterpret_cast<const wchar_t*>(catName.BeginReading()), pArray);
      if (SUCCEEDED(hr)) *_retval = true;

      // Get rid of the old icons
      nsCOMPtr<nsIRunnable> event =
          new mozilla::widget::AsyncDeleteAllFaviconsFromDisk(true);
      mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

      return NS_OK;
    } break;
  }
  return NS_OK;
}

NS_IMETHODIMP LegacyJumpListBuilder::AbortListBuild() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr) return NS_ERROR_NOT_AVAILABLE;

  RefPtr<ICustomDestinationList> jumpListMgr = mJumpListMgr.Resolve();
  if (!jumpListMgr) {
    return NS_ERROR_UNEXPECTED;
  }

  jumpListMgr->AbortList();
  sBuildingList = false;

  return NS_OK;
}

NS_IMETHODIMP LegacyJumpListBuilder::CommitListBuild(
    nsILegacyJumpListCommittedCallback* aCallback) {
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr) return NS_ERROR_NOT_AVAILABLE;

  // Also holds a strong reference to this to prevent use-after-free.
  RefPtr<detail::DoneCommitListBuildCallback> callback =
      new detail::DoneCommitListBuildCallback(aCallback, this);

  // The builder has a strong reference in the callback already, so we do not
  // need to do it for this runnable again.
  RefPtr<nsIRunnable> event =
      NewNonOwningRunnableMethod<RefPtr<detail::DoneCommitListBuildCallback>>(
          "LegacyJumpListBuilder::DoCommitListBuild", this,
          &LegacyJumpListBuilder::DoCommitListBuild, std::move(callback));
  Unused << mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

  return NS_OK;
}

void LegacyJumpListBuilder::DoCommitListBuild(
    RefPtr<detail::DoneCommitListBuildCallback> aCallback) {
  // Since we're invoking COM interfaces to talk to the shell on a background
  // thread, we need to be running inside a multithreaded apartment.
  mscom::MTARegion mta;
  MOZ_ASSERT(mta.IsValid());

  ReentrantMonitorAutoEnter lock(mMonitor);
  MOZ_ASSERT(mJumpListMgr);
  MOZ_ASSERT(aCallback);

  HRESULT hr = E_UNEXPECTED;
  auto onExit = MakeScopeExit([&hr, &aCallback]() {
    // XXX We might want some specific error data here.
    aCallback->SetResult(SUCCEEDED(hr));
    Unused << NS_DispatchToMainThread(aCallback);
  });

  RefPtr<ICustomDestinationList> jumpListMgr = mJumpListMgr.Resolve();
  if (!jumpListMgr) {
    return;
  }

  hr = jumpListMgr->CommitList();
  sBuildingList = false;

  if (SUCCEEDED(hr)) {
    mHasCommit = true;
  }
}

NS_IMETHODIMP LegacyJumpListBuilder::DeleteActiveList(bool* _retval) {
  *_retval = false;

  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr) return NS_ERROR_NOT_AVAILABLE;

  if (sBuildingList) {
    AbortListBuild();
  }

  RefPtr<ICustomDestinationList> jumpListMgr = mJumpListMgr.Resolve();
  if (!jumpListMgr) {
    return NS_ERROR_UNEXPECTED;
  }

  if (SUCCEEDED(jumpListMgr->DeleteList(mAppUserModelId.get()))) {
    *_retval = true;
  }

  return NS_OK;
}

/* internal */

bool LegacyJumpListBuilder::IsSeparator(nsCOMPtr<nsILegacyJumpListItem>& item) {
  int16_t type;
  item->GetType(&type);
  if (NS_FAILED(item->GetType(&type))) return false;

  if (type == nsILegacyJumpListItem::JUMPLIST_ITEM_SEPARATOR) return true;
  return false;
}

// RemoveIconCacheAndGetJumplistShortcutURIs - does multiple things to
// avoid unnecessary extra XPCOM incantations. For each object in the input
// array,  if it's an IShellLinkW, this deletes the cached icon and adds the
// url param to a list of URLs to be removed from the places database.
void LegacyJumpListBuilder::RemoveIconCacheAndGetJumplistShortcutURIs(
    IObjectArray* aObjArray, nsTArray<nsString>& aURISpecs) {
  MOZ_ASSERT(!NS_IsMainThread());

  // Early return here just in case some versions of Windows don't populate this
  if (!aObjArray) {
    return;
  }

  uint32_t count = 0;
  aObjArray->GetCount(&count);

  for (uint32_t idx = 0; idx < count; idx++) {
    RefPtr<IShellLinkW> pLink;

    if (FAILED(aObjArray->GetAt(idx, IID_IShellLinkW,
                                static_cast<void**>(getter_AddRefs(pLink))))) {
      continue;
    }

    wchar_t buf[MAX_PATH];
    HRESULT hres = pLink->GetArguments(buf, MAX_PATH);
    if (SUCCEEDED(hres)) {
      LPWSTR* arglist;
      int32_t numArgs;

      arglist = ::CommandLineToArgvW(buf, &numArgs);
      if (arglist && numArgs > 0) {
        nsString spec(arglist[0]);
        aURISpecs.AppendElement(std::move(spec));
        ::LocalFree(arglist);
      }
    }

    int iconIdx = 0;
    hres = pLink->GetIconLocation(buf, MAX_PATH, &iconIdx);
    if (SUCCEEDED(hres)) {
      nsDependentString spec(buf);
      DeleteIconFromDisk(spec);
    }
  }
}

void LegacyJumpListBuilder::DeleteIconFromDisk(const nsAString& aPath) {
  MOZ_ASSERT(!NS_IsMainThread());

  // Check that we aren't deleting some arbitrary file that is not an icon
  if (StringTail(aPath, 4).LowerCaseEqualsASCII(".ico")) {
    // Construct the parent path of the passed in path
    nsCOMPtr<nsIFile> icoFile;
    nsresult rv = NS_NewLocalFile(aPath, true, getter_AddRefs(icoFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    icoFile->Remove(false);
  }
}

NS_IMETHODIMP LegacyJumpListBuilder::Observe(nsISupports* aSubject,
                                             const char* aTopic,
                                             const char16_t* aData) {
  NS_ENSURE_ARG_POINTER(aTopic);
  if (strcmp(aTopic, TOPIC_PROFILE_BEFORE_CHANGE) == 0) {
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      observerService->RemoveObserver(this, TOPIC_PROFILE_BEFORE_CHANGE);
    }
    mIOThread->Shutdown();
    // Clear out mJumpListMgr, as MSCOM services won't be available soon.
    ReentrantMonitorAutoEnter lock(mMonitor);
    mJumpListMgr = nullptr;
  } else if (strcmp(aTopic, "nsPref:changed") == 0 &&
             nsDependentString(aData).EqualsASCII(kPrefTaskbarEnabled)) {
    bool enabled = Preferences::GetBool(kPrefTaskbarEnabled, true);
    if (!enabled) {
      nsCOMPtr<nsIRunnable> event =
          new mozilla::widget::AsyncDeleteAllFaviconsFromDisk();
      mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
    }
  } else if (strcmp(aTopic, TOPIC_CLEAR_PRIVATE_DATA) == 0) {
    // Delete JumpListCache icons from Disk, if any.
    nsCOMPtr<nsIRunnable> event =
        new mozilla::widget::AsyncDeleteAllFaviconsFromDisk(false);
    mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }
  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla

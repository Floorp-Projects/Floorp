/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JumpListBuilder.h"

#include "nsError.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsArrayUtils.h"
#include "nsIMutableArray.h"
#include "nsWidgetsCID.h"
#include "WinTaskbar.h"
#include "nsDirectoryServiceUtils.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/Preferences.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "mozilla/LazyIdleThread.h"
#include "nsIObserverService.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Promise.h"

#include <shellapi.h>
#include "WinUtils.h"

using mozilla::dom::Promise;

// The amount of time, in milliseconds, that our IO thread will stay alive after the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

namespace mozilla {
namespace widget {

// defined in WinTaskbar.cpp
extern const wchar_t *gMozillaJumpListIDGeneric;

Atomic<bool> JumpListBuilder::sBuildingList(false);
const char kPrefTaskbarEnabled[] = "browser.taskbar.lists.enabled";

NS_IMPL_ISUPPORTS(JumpListBuilder, nsIJumpListBuilder, nsIObserver)
#define TOPIC_PROFILE_BEFORE_CHANGE "profile-before-change"
#define TOPIC_CLEAR_PRIVATE_DATA "clear-private-data"


namespace detail {

class DoneCommitListBuildCallback final : public nsIRunnable
{
  NS_DECL_THREADSAFE_ISUPPORTS

public:
  DoneCommitListBuildCallback(nsIJumpListCommittedCallback* aCallback,
                              JumpListBuilder* aBuilder)
    : mCallback(aCallback)
    , mBuilder(aBuilder)
    , mResult(false)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mCallback) {
      Unused << mCallback->Done(mResult);
    }
    // Ensure we are releasing on the main thread.
    Destroy();
    return NS_OK;
  }

  void SetResult(bool aResult)
  {
    mResult = aResult;
  }

private:
  ~DoneCommitListBuildCallback()
  {
    // Destructor does not always call on the main thread.
    MOZ_ASSERT(!mCallback);
    MOZ_ASSERT(!mBuilder);
  }

  void Destroy()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mCallback = nullptr;
    mBuilder = nullptr;
  }

  // These two references MUST be released on the main thread.
  RefPtr<nsIJumpListCommittedCallback> mCallback;
  RefPtr<JumpListBuilder> mBuilder;
  bool mResult;
};

NS_IMPL_ISUPPORTS(DoneCommitListBuildCallback, nsIRunnable);

} // namespace detail


JumpListBuilder::JumpListBuilder() :
  mMaxItems(0),
  mHasCommit(false),
  mMonitor("JumpListBuilderMonitor")
{
  ::CoInitialize(nullptr);

  CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER,
                   IID_ICustomDestinationList, getter_AddRefs(mJumpListMgr));

  // Make a lazy thread for any IO
  mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                                 NS_LITERAL_CSTRING("Jump List"),
                                 LazyIdleThread::ManualShutdown);
  Preferences::AddStrongObserver(this, kPrefTaskbarEnabled);

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    observerService->AddObserver(this, TOPIC_PROFILE_BEFORE_CHANGE, false);
    observerService->AddObserver(this, TOPIC_CLEAR_PRIVATE_DATA, false);
  }
}

JumpListBuilder::~JumpListBuilder()
{
  Preferences::RemoveObserver(this, kPrefTaskbarEnabled);
  mJumpListMgr = nullptr;
  ::CoUninitialize();
}

NS_IMETHODIMP JumpListBuilder::GetAvailable(int16_t *aAvailable)
{
  *aAvailable = false;

  ReentrantMonitorAutoEnter lock(mMonitor);
  if (mJumpListMgr)
    *aAvailable = true;

  return NS_OK;
}

NS_IMETHODIMP JumpListBuilder::GetIsListCommitted(bool *aCommit)
{
  *aCommit = mHasCommit;

  return NS_OK;
}

NS_IMETHODIMP JumpListBuilder::GetMaxListItems(int16_t *aMaxItems)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  *aMaxItems = 0;

  if (sBuildingList) {
    *aMaxItems = mMaxItems;
    return NS_OK;
  }

  IObjectArray *objArray;
  if (SUCCEEDED(mJumpListMgr->BeginList(&mMaxItems, IID_PPV_ARGS(&objArray)))) {
    *aMaxItems = mMaxItems;

    if (objArray)
      objArray->Release();

    mJumpListMgr->AbortList();
  }

  return NS_OK;
}

NS_IMETHODIMP JumpListBuilder::InitListBuild(JSContext* aCx,
                                             Promise** aPromise)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsIGlobalObject* globalObject =
    xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));

  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod<StoreCopyPassByRRef<RefPtr<Promise>>>("InitListBuild",
                                                            this,
                                                            &JumpListBuilder::DoInitListBuild,
                                                            promise);
  nsresult rv = mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);
  return NS_OK;
}

void JumpListBuilder::DoInitListBuild(RefPtr<Promise>&& aPromise)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  MOZ_ASSERT(mJumpListMgr);

  if(sBuildingList) {
    AbortListBuild();
  }

  nsTArray<nsString> urisToRemove;
  RefPtr<IObjectArray> objArray;
  HRESULT hr = mJumpListMgr->BeginList(&mMaxItems,
                                       IID_PPV_ARGS(static_cast<IObjectArray**>
                                                    (getter_AddRefs(objArray))));
  // The returned objArray of removed items are for manually removed items.
  // This does not return items which are removed because they were previously
  // part of the jump list but are no longer part of the jump list.
  if (SUCCEEDED(hr) && objArray) {
    sBuildingList = true;
    RemoveIconCacheAndGetJumplistShortcutURIs(objArray, urisToRemove);
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction("InitListBuildResolve",
                                                 [urisToRemove = std::move(urisToRemove),
                                                  promise = std::move(aPromise)]() {
    promise->MaybeResolve(urisToRemove);
  }));
}

// Ensures that we have no old ICO files left in the jump list cache
nsresult JumpListBuilder::RemoveIconCacheForAllItems()
{
  // Construct the path of our jump list cache
  nsCOMPtr<nsIFile> jumpListCacheDir;
  nsresult rv = NS_GetSpecialDirectory("ProfLDS",
                                       getter_AddRefs(jumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = jumpListCacheDir->AppendNative(nsDependentCString(
                         mozilla::widget::FaviconHelper::kJumpListCacheDir));
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
    if (NS_FAILED(currFile->GetPath(path)))
      continue;

    if (StringTail(path, 4).LowerCaseEqualsASCII(".ico")) {
      // Check if the cached ICO file exists
      bool exists;
      if (NS_FAILED(currFile->Exists(&exists)) || !exists)
        continue;

      // We found an ICO file that exists, so we should remove it
      currFile->Remove(false);
    }
  } while(true);

  return NS_OK;
}

NS_IMETHODIMP JumpListBuilder::AddListToBuild(int16_t aCatType, nsIArray *items, const nsAString &catName, bool *_retval)
{
  nsresult rv;

  *_retval = false;

  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  switch(aCatType) {
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_TASKS:
    {
      NS_ENSURE_ARG_POINTER(items);

      HRESULT hr;
      RefPtr<IObjectCollection> collection;
      hr = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                            CLSCTX_INPROC_SERVER, IID_IObjectCollection,
                            getter_AddRefs(collection));
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      // Build the list
      uint32_t length;
      items->GetLength(&length);
      for (uint32_t i = 0; i < length; ++i) {
        nsCOMPtr<nsIJumpListItem> item = do_QueryElementAt(items, i);
        if (!item)
          continue;
        // Check for separators
        if (IsSeparator(item)) {
          RefPtr<IShellLinkW> link;
          rv = JumpListSeparator::GetSeparator(link);
          if (NS_FAILED(rv))
            return rv;
          collection->AddObject(link);
          continue;
        }
        // These should all be ShellLinks
        RefPtr<IShellLinkW> link;
        rv = JumpListShortcut::GetShellLink(item, link, mIOThread);
        if (NS_FAILED(rv))
          return rv;
        collection->AddObject(link);
      }

      // We need IObjectArray to submit
      RefPtr<IObjectArray> pArray;
      hr = collection->QueryInterface(IID_IObjectArray, getter_AddRefs(pArray));
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      // Add the tasks
      hr = mJumpListMgr->AddUserTasks(pArray);
      if (SUCCEEDED(hr))
        *_retval = true;
      return NS_OK;
    }
    break;
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_RECENT:
    {
      if (SUCCEEDED(mJumpListMgr->AppendKnownCategory(KDC_RECENT)))
        *_retval = true;
      return NS_OK;
    }
    break;
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_FREQUENT:
    {
      if (SUCCEEDED(mJumpListMgr->AppendKnownCategory(KDC_FREQUENT)))
        *_retval = true;
      return NS_OK;
    }
    break;
    case nsIJumpListBuilder::JUMPLIST_CATEGORY_CUSTOMLIST:
    {
      NS_ENSURE_ARG_POINTER(items);

      if (catName.IsEmpty())
        return NS_ERROR_INVALID_ARG;

      HRESULT hr;
      RefPtr<IObjectCollection> collection;
      hr = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                            CLSCTX_INPROC_SERVER, IID_IObjectCollection,
                            getter_AddRefs(collection));
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      uint32_t length;
      items->GetLength(&length);
      for (uint32_t i = 0; i < length; ++i) {
        nsCOMPtr<nsIJumpListItem> item = do_QueryElementAt(items, i);
        if (!item)
          continue;
        int16_t type;
        if (NS_FAILED(item->GetType(&type)))
          continue;
        switch(type) {
          case nsIJumpListItem::JUMPLIST_ITEM_SEPARATOR:
          {
            RefPtr<IShellLinkW> shellItem;
            rv = JumpListSeparator::GetSeparator(shellItem);
            if (NS_FAILED(rv))
              return rv;
            collection->AddObject(shellItem);
          }
          break;
          case nsIJumpListItem::JUMPLIST_ITEM_LINK:
          {
            RefPtr<IShellItem2> shellItem;
            rv = JumpListLink::GetShellItem(item, shellItem);
            if (NS_FAILED(rv))
              return rv;
            collection->AddObject(shellItem);
          }
          break;
          case nsIJumpListItem::JUMPLIST_ITEM_SHORTCUT:
          {
            RefPtr<IShellLinkW> shellItem;
            rv = JumpListShortcut::GetShellLink(item, shellItem, mIOThread);
            if (NS_FAILED(rv))
              return rv;
            collection->AddObject(shellItem);
          }
          break;
        }
      }

      // We need IObjectArray to submit
      RefPtr<IObjectArray> pArray;
      hr = collection->QueryInterface(IID_IObjectArray, (LPVOID*)&pArray);
      if (FAILED(hr))
        return NS_ERROR_UNEXPECTED;

      // Add the tasks
      hr = mJumpListMgr->AppendCategory(reinterpret_cast<const wchar_t*>(catName.BeginReading()), pArray);
      if (SUCCEEDED(hr))
        *_retval = true;

      // Get rid of the old icons
      nsCOMPtr<nsIRunnable> event =
        new mozilla::widget::AsyncDeleteAllFaviconsFromDisk(true);
      mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

      return NS_OK;
    }
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP JumpListBuilder::AbortListBuild()
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  mJumpListMgr->AbortList();
  sBuildingList = false;

  return NS_OK;
}

NS_IMETHODIMP JumpListBuilder::CommitListBuild(nsIJumpListCommittedCallback* aCallback)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  // Also holds a strong reference to this to prevent use-after-free.
  RefPtr<detail::DoneCommitListBuildCallback> callback =
    new detail::DoneCommitListBuildCallback(aCallback, this);

  // The builder has a strong reference in the callback already, so we do not
  // need to do it for this runnable again.
  RefPtr<nsIRunnable> event =
    NewNonOwningRunnableMethod<RefPtr<detail::DoneCommitListBuildCallback>>
      ("JumpListBuilder::DoCommitListBuild", this,
       &JumpListBuilder::DoCommitListBuild, std::move(callback));
  Unused << mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

  return NS_OK;
}

void JumpListBuilder::DoCommitListBuild(RefPtr<detail::DoneCommitListBuildCallback> aCallback)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  MOZ_ASSERT(mJumpListMgr);
  MOZ_ASSERT(aCallback);

  HRESULT hr = mJumpListMgr->CommitList();
  sBuildingList = false;

  if (SUCCEEDED(hr)) {
    mHasCommit = true;
  }

  // XXX We might want some specific error data here.
  aCallback->SetResult(SUCCEEDED(hr));
  Unused << NS_DispatchToMainThread(aCallback);
}

NS_IMETHODIMP JumpListBuilder::DeleteActiveList(bool *_retval)
{
  *_retval = false;

  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!mJumpListMgr)
    return NS_ERROR_NOT_AVAILABLE;

  if(sBuildingList)
    AbortListBuild();

  nsAutoString uid;
  if (!WinTaskbar::GetAppUserModelID(uid))
    return NS_OK;

  if (SUCCEEDED(mJumpListMgr->DeleteList(uid.get())))
    *_retval = true;

  return NS_OK;
}

/* internal */

bool JumpListBuilder::IsSeparator(nsCOMPtr<nsIJumpListItem>& item)
{
  int16_t type;
  item->GetType(&type);
  if (NS_FAILED(item->GetType(&type)))
    return false;

  if (type == nsIJumpListItem::JUMPLIST_ITEM_SEPARATOR)
    return true;
  return false;
}

// RemoveIconCacheAndGetJumplistShortcutURIs - does multiple things to
// avoid unnecessary extra XPCOM incantations. For each object in the input
// array,  if it's an IShellLinkW, this deletes the cached icon and adds the
// url param to a list of URLs to be removed from the places database.
void JumpListBuilder::RemoveIconCacheAndGetJumplistShortcutURIs(IObjectArray *aObjArray,
                                                                nsTArray<nsString>& aURISpecs)
{
  MOZ_ASSERT(!NS_IsMainThread());

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
      LPWSTR *arglist;
      int32_t numArgs;

      arglist = ::CommandLineToArgvW(buf, &numArgs);
      if(arglist && numArgs > 0) {
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

void JumpListBuilder::DeleteIconFromDisk(const nsAString& aPath)
{
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

NS_IMETHODIMP JumpListBuilder::Observe(nsISupports* aSubject,
                                       const char* aTopic,
                                       const char16_t* aData)
{
  NS_ENSURE_ARG_POINTER(aTopic);
  if (strcmp(aTopic, TOPIC_PROFILE_BEFORE_CHANGE) == 0) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      observerService->RemoveObserver(this, TOPIC_PROFILE_BEFORE_CHANGE);
    }
    mIOThread->Shutdown();
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

} // namespace widget
} // namespace mozilla


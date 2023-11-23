/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shellapi.h>
#include "JumpListBuilder.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WindowsJumpListShortcutDescriptionBinding.h"
#include "mozilla/mscom/EnsureMTA.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "WinUtils.h"

using mozilla::dom::Promise;
using mozilla::dom::WindowsJumpListShortcutDescription;

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS(JumpListBuilder, nsIJumpListBuilder, nsIObserver)

#define TOPIC_PROFILE_BEFORE_CHANGE "profile-before-change"
#define TOPIC_CLEAR_PRIVATE_DATA "clear-private-data"

// The amount of time, in milliseconds, that our IO thread will stay alive after
// the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

const char kPrefTaskbarEnabled[] = "browser.taskbar.lists.enabled";

/**
 * A wrapper around a ICustomDestinationList that implements the JumpListBackend
 * interface. This is an implementation of JumpListBackend that actually causes
 * items to appear in a Windows jump list.
 */
class NativeJumpListBackend : public JumpListBackend {
  // We use NS_INLINE_DECL_REFCOUNTING_ONEVENTTARGET because this
  // class might be destroyed on a different thread than the one it
  // was created on, since it's maintained by a LazyIdleThread.
  //
  // This is a workaround for bug 1648031.
  NS_INLINE_DECL_REFCOUNTING_ONEVENTTARGET(JumpListBackend, override)

  NativeJumpListBackend() {
    MOZ_ASSERT(!NS_IsMainThread());

    mscom::EnsureMTA([&]() {
      RefPtr<ICustomDestinationList> destList;
      HRESULT hr = ::CoCreateInstance(
          CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER,
          IID_ICustomDestinationList, getter_AddRefs(destList));
      if (FAILED(hr)) {
        return;
      }

      mWindowsDestList = destList;
    });
  }

  virtual bool IsAvailable() override {
    MOZ_ASSERT(!NS_IsMainThread());
    return mWindowsDestList != nullptr;
  }

  virtual HRESULT SetAppID(LPCWSTR pszAppID) override {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mWindowsDestList);

    return mWindowsDestList->SetAppID(pszAppID);
  }

  virtual HRESULT BeginList(UINT* pcMinSlots, REFIID riid,
                            void** ppv) override {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mWindowsDestList);

    return mWindowsDestList->BeginList(pcMinSlots, riid, ppv);
  }

  virtual HRESULT AddUserTasks(IObjectArray* poa) override {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mWindowsDestList);

    return mWindowsDestList->AddUserTasks(poa);
  }

  virtual HRESULT AppendCategory(LPCWSTR pszCategory,
                                 IObjectArray* poa) override {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mWindowsDestList);

    return mWindowsDestList->AppendCategory(pszCategory, poa);
  }

  virtual HRESULT CommitList() override {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mWindowsDestList);

    return mWindowsDestList->CommitList();
  }

  virtual HRESULT AbortList() override {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mWindowsDestList);

    return mWindowsDestList->AbortList();
  }

  virtual HRESULT DeleteList(LPCWSTR pszAppID) override {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mWindowsDestList);

    return mWindowsDestList->DeleteList(pszAppID);
  }

  virtual HRESULT AppendKnownCategory(KNOWNDESTCATEGORY category) override {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(mWindowsDestList);

    return mWindowsDestList->AppendKnownCategory(category);
  }

 protected:
  virtual ~NativeJumpListBackend() override{};

 private:
  RefPtr<ICustomDestinationList> mWindowsDestList;
};

JumpListBuilder::JumpListBuilder(const nsAString& aAppUserModelId,
                                 RefPtr<JumpListBackend> aTestingBackend) {
  MOZ_ASSERT(NS_IsMainThread());

  mAppUserModelId.Assign(aAppUserModelId);

  Preferences::AddStrongObserver(this, kPrefTaskbarEnabled);

  // Make a lazy thread for any IO.
  mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS, "Jump List",
                                 LazyIdleThread::ManualShutdown);

  nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
  if (observerService) {
    observerService->AddObserver(this, TOPIC_PROFILE_BEFORE_CHANGE, false);
    observerService->AddObserver(this, TOPIC_CLEAR_PRIVATE_DATA, false);
  }

  nsCOMPtr<nsIRunnable> runnable;

  if (aTestingBackend) {
    // Dispatch a task that hands a reference to the testing backend
    // to the background thread. The testing backend was probably
    // constructed on the main thread, and is responsible for doing
    // any locking as well as cleanup.
    runnable = NewRunnableMethod<RefPtr<JumpListBackend>>(
        "SetupTestingBackend", this, &JumpListBuilder::DoSetupTestingBackend,
        aTestingBackend);

  } else {
    // Dispatch a task that constructs the native jump list backend.
    runnable = NewRunnableMethod("SetupBackend", this,
                                 &JumpListBuilder::DoSetupBackend);
  }

  mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);

  // MSIX packages explicitly do not support setting the appid from within
  // the app, as it is set in the package manifest instead.
  if (!mozilla::widget::WinUtils::HasPackageIdentity()) {
    mIOThread->Dispatch(
        NewRunnableMethod<nsString>(
            "SetAppID", this, &JumpListBuilder::DoSetAppID, aAppUserModelId),
        NS_DISPATCH_NORMAL);
  }
}

JumpListBuilder::~JumpListBuilder() {
  Preferences::RemoveObserver(this, kPrefTaskbarEnabled);
}

void JumpListBuilder::DoSetupTestingBackend(
    RefPtr<JumpListBackend> aTestingBackend) {
  MOZ_ASSERT(!NS_IsMainThread());
  mJumpListBackend = aTestingBackend;
}

void JumpListBuilder::DoSetupBackend() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mJumpListBackend);
  mJumpListBackend = new NativeJumpListBackend();
}

void JumpListBuilder::DoShutdownBackend() {
  MOZ_ASSERT(!NS_IsMainThread());
  mJumpListBackend = nullptr;
}

void JumpListBuilder::DoSetAppID(nsString aAppUserModelID) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mJumpListBackend);
  mJumpListBackend->SetAppID(aAppUserModelID.get());
}

NS_IMETHODIMP
JumpListBuilder::ObtainAndCacheFavicon(nsIURI* aFaviconURI,
                                       nsAString& aCachedIconPath) {
  MOZ_ASSERT(NS_IsMainThread());
  nsString iconFilePath;
  nsresult rv = mozilla::widget::FaviconHelper::ObtainCachedIconFile(
      aFaviconURI, iconFilePath, mIOThread, false);
  NS_ENSURE_SUCCESS(rv, rv);

  aCachedIconPath = iconFilePath;
  return NS_OK;
}

NS_IMETHODIMP
JumpListBuilder::IsAvailable(JSContext* aCx, Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(mIOThread);

  ErrorResult result;
  RefPtr<Promise> promise =
      Promise::Create(xpc::CurrentNativeGlobal(aCx), result);

  if (MOZ_UNLIKELY(result.Failed())) {
    return result.StealNSResult();
  }

  nsMainThreadPtrHandle<Promise> promiseHolder(
      new nsMainThreadPtrHolder<Promise>("JumpListBuilder::IsAvailable promise",
                                         promise));

  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod<nsMainThreadPtrHandle<Promise>>(
          "IsAvailable", this, &JumpListBuilder::DoIsAvailable,
          std::move(promiseHolder));
  nsresult rv = mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
JumpListBuilder::CheckForRemovals(JSContext* aCx, Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(mIOThread);

  ErrorResult result;
  RefPtr<Promise> promise =
      Promise::Create(xpc::CurrentNativeGlobal(aCx), result);

  if (MOZ_UNLIKELY(result.Failed())) {
    return result.StealNSResult();
  }

  nsMainThreadPtrHandle<Promise> promiseHolder(
      new nsMainThreadPtrHolder<Promise>(
          "JumpListBuilder::CheckForRemovals promise", promise));

  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod<nsMainThreadPtrHandle<Promise>>(
          "CheckForRemovals", this, &JumpListBuilder::DoCheckForRemovals,
          std::move(promiseHolder));

  nsresult rv = mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);

  return NS_OK;
}

NS_IMETHODIMP
JumpListBuilder::PopulateJumpList(
    const nsTArray<JS::Value>& aTaskDescriptions, const nsAString& aCustomTitle,
    const nsTArray<JS::Value>& aCustomDescriptions, JSContext* aCx,
    Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(mIOThread);

  if (aCustomDescriptions.Length() && aCustomTitle.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // Get rid of the old icons
  nsCOMPtr<nsIRunnable> event =
      new mozilla::widget::AsyncDeleteAllFaviconsFromDisk(true);
  mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

  nsTArray<WindowsJumpListShortcutDescription> taskDescs;
  for (auto& jsval : aTaskDescriptions) {
    JS::Rooted<JS::Value> rootedVal(aCx);
    if (NS_WARN_IF(!dom::ToJSValue(aCx, jsval, &rootedVal))) {
      return NS_ERROR_INVALID_ARG;
    }

    WindowsJumpListShortcutDescription desc;
    if (!desc.Init(aCx, rootedVal)) {
      return NS_ERROR_INVALID_ARG;
    }

    taskDescs.AppendElement(std::move(desc));
  }

  nsTArray<WindowsJumpListShortcutDescription> customDescs;
  for (auto& jsval : aCustomDescriptions) {
    JS::Rooted<JS::Value> rootedVal(aCx);
    if (NS_WARN_IF(!dom::ToJSValue(aCx, jsval, &rootedVal))) {
      return NS_ERROR_INVALID_ARG;
    }

    WindowsJumpListShortcutDescription desc;
    if (!desc.Init(aCx, rootedVal)) {
      return NS_ERROR_INVALID_ARG;
    }

    customDescs.AppendElement(std::move(desc));
  }

  ErrorResult result;
  RefPtr<Promise> promise =
      Promise::Create(xpc::CurrentNativeGlobal(aCx), result);

  if (MOZ_UNLIKELY(result.Failed())) {
    return result.StealNSResult();
  }

  nsMainThreadPtrHandle<Promise> promiseHolder(
      new nsMainThreadPtrHolder<Promise>(
          "JumpListBuilder::PopulateJumpList promise", promise));

  nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod<
      StoreCopyPassByRRef<nsTArray<WindowsJumpListShortcutDescription>>,
      nsString,
      StoreCopyPassByRRef<nsTArray<WindowsJumpListShortcutDescription>>,
      nsMainThreadPtrHandle<Promise>>(
      "PopulateJumpList", this, &JumpListBuilder::DoPopulateJumpList,
      std::move(taskDescs), aCustomTitle, std::move(customDescs),
      std::move(promiseHolder));
  nsresult rv = mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);

  return NS_OK;
}

NS_IMETHODIMP
JumpListBuilder::ClearJumpList(JSContext* aCx, Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(mIOThread);

  ErrorResult result;
  RefPtr<Promise> promise =
      Promise::Create(xpc::CurrentNativeGlobal(aCx), result);

  if (MOZ_UNLIKELY(result.Failed())) {
    return result.StealNSResult();
  }

  nsMainThreadPtrHandle<Promise> promiseHolder(
      new nsMainThreadPtrHolder<Promise>(
          "JumpListBuilder::ClearJumpList promise", promise));

  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod<nsMainThreadPtrHandle<Promise>>(
          "ClearJumpList", this, &JumpListBuilder::DoClearJumpList,
          std::move(promiseHolder));
  nsresult rv = mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(aPromise);
  return NS_OK;
}

void JumpListBuilder::DoIsAvailable(
    const nsMainThreadPtrHandle<Promise>& aPromiseHolder) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aPromiseHolder);

  if (!mJumpListBackend) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "DoIsAvailable", [promiseHolder = std::move(aPromiseHolder)]() {
          promiseHolder.get()->MaybeResolve(false);
        }));
    return;
  }

  bool isAvailable = mJumpListBackend->IsAvailable();

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "DoIsAvailable",
      [promiseHolder = std::move(aPromiseHolder), isAvailable]() {
        promiseHolder.get()->MaybeResolve(isAvailable);
      }));
}

void JumpListBuilder::DoCheckForRemovals(
    const nsMainThreadPtrHandle<Promise>& aPromiseHolder) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aPromiseHolder);

  nsresult rv = NS_ERROR_UNEXPECTED;

  auto errorHandler = MakeScopeExit([&aPromiseHolder, &rv]() {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "DoCheckForRemovals",
        [promiseHolder = std::move(aPromiseHolder), rv]() {
          promiseHolder.get()->MaybeReject(rv);
        }));
  });

  MOZ_ASSERT(mJumpListBackend);
  if (!mJumpListBackend) {
    return;
  }

  // Abort any ongoing list building that might not have been committed,
  // otherwise BeginList will give us problems.
  Unused << mJumpListBackend->AbortList();

  nsTArray<nsString> urisToRemove;
  RefPtr<IObjectArray> objArray;
  uint32_t maxItems = 0;

  HRESULT hr = mJumpListBackend->BeginList(
      &maxItems,
      IID_PPV_ARGS(static_cast<IObjectArray**>(getter_AddRefs(objArray))));

  if (FAILED(hr)) {
    rv = NS_ERROR_UNEXPECTED;
    return;
  }

  RemoveIconCacheAndGetJumplistShortcutURIs(objArray, urisToRemove);

  // We began a list in order to get the removals, which we can now abort.
  Unused << mJumpListBackend->AbortList();

  errorHandler.release();

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "DoCheckForRemovals", [urisToRemove = std::move(urisToRemove),
                             promiseHolder = std::move(aPromiseHolder)]() {
        promiseHolder.get()->MaybeResolve(urisToRemove);
      }));
}

void JumpListBuilder::DoPopulateJumpList(
    const nsTArray<WindowsJumpListShortcutDescription>&& aTaskDescriptions,
    const nsAString& aCustomTitle,
    const nsTArray<WindowsJumpListShortcutDescription>&& aCustomDescriptions,
    const nsMainThreadPtrHandle<Promise>& aPromiseHolder) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aPromiseHolder);

  nsresult rv = NS_ERROR_UNEXPECTED;

  auto errorHandler = MakeScopeExit([&aPromiseHolder, &rv]() {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "DoPopulateJumpList",
        [promiseHolder = std::move(aPromiseHolder), rv]() {
          promiseHolder.get()->MaybeReject(rv);
        }));
  });

  MOZ_ASSERT(mJumpListBackend);
  if (!mJumpListBackend) {
    return;
  }

  // Abort any ongoing list building that might not have been committed,
  // otherwise BeginList will give us problems.
  Unused << mJumpListBackend->AbortList();

  nsTArray<nsString> urisToRemove;
  RefPtr<IObjectArray> objArray;
  uint32_t maxItems = 0;

  HRESULT hr = mJumpListBackend->BeginList(
      &maxItems,
      IID_PPV_ARGS(static_cast<IObjectArray**>(getter_AddRefs(objArray))));

  if (FAILED(hr)) {
    rv = NS_ERROR_UNEXPECTED;
    return;
  }

  if (urisToRemove.Length()) {
    // It'd be nice if we could return a more descriptive error here so that
    // the caller knows that they should have called checkForRemovals first.
    rv = NS_ERROR_UNEXPECTED;
    return;
  }

  if (aTaskDescriptions.Length()) {
    RefPtr<IObjectCollection> taskCollection;
    hr = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                          CLSCTX_INPROC_SERVER, IID_IObjectCollection,
                          getter_AddRefs(taskCollection));
    if (FAILED(hr)) {
      rv = NS_ERROR_UNEXPECTED;
      return;
    }

    // Start by building the task list
    for (auto& desc : aTaskDescriptions) {
      // These should all be ShellLinks
      RefPtr<IShellLinkW> link;
      rv = GetShellLinkFromDescription(desc, link);
      if (NS_FAILED(rv)) {
        // Let the errorHandler deal with this.
        return;
      }
      taskCollection->AddObject(link);
    }

    RefPtr<IObjectArray> pTaskArray;
    hr = taskCollection->QueryInterface(IID_IObjectArray,
                                        getter_AddRefs(pTaskArray));

    if (FAILED(hr)) {
      rv = NS_ERROR_UNEXPECTED;
      return;
    }

    hr = mJumpListBackend->AddUserTasks(pTaskArray);

    if (FAILED(hr)) {
      rv = NS_ERROR_FAILURE;
      return;
    }
  }

  if (aCustomDescriptions.Length()) {
    // Then build the custom list
    RefPtr<IObjectCollection> customCollection;
    hr = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                          CLSCTX_INPROC_SERVER, IID_IObjectCollection,
                          getter_AddRefs(customCollection));
    if (FAILED(hr)) {
      rv = NS_ERROR_UNEXPECTED;
      return;
    }

    for (auto& desc : aCustomDescriptions) {
      // These should all be ShellLinks
      RefPtr<IShellLinkW> link;
      rv = GetShellLinkFromDescription(desc, link);
      if (NS_FAILED(rv)) {
        // Let the errorHandler deal with this.
        return;
      }
      customCollection->AddObject(link);
    }

    RefPtr<IObjectArray> pCustomArray;
    hr = customCollection->QueryInterface(IID_IObjectArray,
                                          getter_AddRefs(pCustomArray));
    if (FAILED(hr)) {
      rv = NS_ERROR_UNEXPECTED;
      return;
    }

    hr = mJumpListBackend->AppendCategory(
        reinterpret_cast<const wchar_t*>(aCustomTitle.BeginReading()),
        pCustomArray);

    if (FAILED(hr)) {
      rv = NS_ERROR_UNEXPECTED;
      return;
    }
  }

  hr = mJumpListBackend->CommitList();
  if (FAILED(hr)) {
    rv = NS_ERROR_FAILURE;
    return;
  }

  errorHandler.release();

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "DoPopulateJumpList", [promiseHolder = std::move(aPromiseHolder)]() {
        promiseHolder.get()->MaybeResolveWithUndefined();
      }));
}

void JumpListBuilder::DoClearJumpList(
    const nsMainThreadPtrHandle<Promise>& aPromiseHolder) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aPromiseHolder);

  if (!mJumpListBackend) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "DoClearJumpList", [promiseHolder = std::move(aPromiseHolder)]() {
          promiseHolder.get()->MaybeReject(NS_ERROR_UNEXPECTED);
        }));
    return;
  }

  if (SUCCEEDED(mJumpListBackend->DeleteList(mAppUserModelId.get()))) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "DoClearJumpList", [promiseHolder = std::move(aPromiseHolder)]() {
          promiseHolder.get()->MaybeResolveWithUndefined();
        }));
  } else {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "DoClearJumpList", [promiseHolder = std::move(aPromiseHolder)]() {
          promiseHolder.get()->MaybeReject(NS_ERROR_FAILURE);
        }));
  }
}

// RemoveIconCacheAndGetJumplistShortcutURIs - does multiple things to
// avoid unnecessary extra XPCOM incantations. For each object in the input
// array,  if it's an IShellLinkW, this deletes the cached icon and adds the
// url param to a list of URLs to be removed from the places database.
void JumpListBuilder::RemoveIconCacheAndGetJumplistShortcutURIs(
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

void JumpListBuilder::DeleteIconFromDisk(const nsAString& aPath) {
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

// Converts a WindowsJumpListShortcutDescription into a IShellLinkW
nsresult JumpListBuilder::GetShellLinkFromDescription(
    const WindowsJumpListShortcutDescription& aDesc,
    RefPtr<IShellLinkW>& aShellLink) {
  MOZ_ASSERT(!NS_IsMainThread());

  HRESULT hr;
  IShellLinkW* psl;

  // Shell links:
  // http://msdn.microsoft.com/en-us/library/bb776891(VS.85).aspx
  // http://msdn.microsoft.com/en-us/library/bb774950(VS.85).aspx

  // Create a IShellLink
  hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                        IID_IShellLinkW, (LPVOID*)&psl);
  if (FAILED(hr)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Store the title of the app
  if (!aDesc.mTitle.IsEmpty()) {
    IPropertyStore* pPropStore = nullptr;
    hr = psl->QueryInterface(IID_IPropertyStore, (LPVOID*)&pPropStore);
    if (FAILED(hr)) {
      return NS_ERROR_UNEXPECTED;
    }

    PROPVARIANT pv;
    ::InitPropVariantFromString(aDesc.mTitle.get(), &pv);

    pPropStore->SetValue(PKEY_Title, pv);
    pPropStore->Commit();
    pPropStore->Release();

    PropVariantClear(&pv);
  }

  // Store the rest of the params
  hr = psl->SetPath(aDesc.mPath.get());

  // According to the documentation at [1], the maximum description length is
  // INFOTIPSIZE, so we copy the const string from the description into a buffer
  // of that maximum size. However, testing reveals that INFOTIPSIZE is still
  // sometimes too large. MAX_PATH seems to work instead.
  //
  // We truncate to MAX_PATH - 1, since nsAutoString's don't include the null
  // character in their capacity calculations, but the string for IShellLinkW
  // description does. So by truncating to MAX_PATH - 1, the full contents of
  // the truncated nsAutoString will be copied into the IShellLink description
  // buffer.
  //
  // [1]:
  // https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ishelllinka-setdescription
  nsAutoString descriptionCopy(aDesc.mDescription.get());
  if (descriptionCopy.Length() >= MAX_PATH) {
    descriptionCopy.Truncate(MAX_PATH - 1);
  }

  hr = psl->SetDescription(descriptionCopy.get());

  if (aDesc.mArguments.WasPassed() && !aDesc.mArguments.Value().IsEmpty()) {
    hr = psl->SetArguments(aDesc.mArguments.Value().get());
  } else {
    hr = psl->SetArguments(L"");
  }

  // Set up the fallback icon in the event that a valid icon URI has
  // not been supplied.
  hr = psl->SetIconLocation(aDesc.mPath.get(), aDesc.mFallbackIconIndex);

  if (aDesc.mIconPath.WasPassed() && !aDesc.mIconPath.Value().IsEmpty()) {
    // Always use the first icon in the ICO file, as our encoded icon only has 1
    // resource
    hr = psl->SetIconLocation(aDesc.mIconPath.Value().get(), 0);
  }

  aShellLink = dont_AddRef(psl);

  return NS_OK;
}

NS_IMETHODIMP
JumpListBuilder::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aTopic);
  if (strcmp(aTopic, TOPIC_PROFILE_BEFORE_CHANGE) == 0) {
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      observerService->RemoveObserver(this, TOPIC_PROFILE_BEFORE_CHANGE);
      observerService->RemoveObserver(this, TOPIC_CLEAR_PRIVATE_DATA);
    }

    mIOThread->Dispatch(NewRunnableMethod("ShutdownBackend", this,
                                          &JumpListBuilder::DoShutdownBackend),
                        NS_DISPATCH_NORMAL);

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

}  // namespace widget
}  // namespace mozilla

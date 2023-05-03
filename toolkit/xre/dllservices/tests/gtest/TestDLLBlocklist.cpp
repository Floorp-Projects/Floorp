/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <winternl.h>

#include <process.h>

#include <array>
#include <functional>

#include "gtest/gtest.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Char16.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/Services.h"
#include "mozilla/WinDllServices.h"
#include "mozilla/WindowsStackCookie.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"
#include "nsWindowsHelpers.h"

static nsString GetFullPath(const nsAString& aLeaf) {
  nsCOMPtr<nsIFile> f;

  EXPECT_TRUE(NS_SUCCEEDED(
      NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR, getter_AddRefs(f))));

  EXPECT_NS_SUCCEEDED(f->Append(aLeaf));

  bool exists;
  EXPECT_TRUE(NS_SUCCEEDED(f->Exists(&exists)) && exists);

  nsString ret;
  EXPECT_NS_SUCCEEDED(f->GetPath(ret));
  return ret;
}

void FlushMainThreadLoop() {
  nsCOMPtr<nsIThread> mainThread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
  ASSERT_NS_SUCCEEDED(rv);

  rv = NS_OK;
  bool processed = true;
  while (processed && NS_SUCCEEDED(rv)) {
    rv = mainThread->ProcessNextEvent(false, &processed);
  }
}

class TestDLLLoadObserver : public nsIObserver {
 public:
  using DLLFilter = std::function<bool(const char16_t*)>;

  explicit TestDLLLoadObserver(DLLFilter dllFilter)
      : mDllFilter(std::move(dllFilter)),
        mMainThreadNotificationsCount(0),
        mNonMainThreadNotificationsCount(0){};

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    if (!mDllFilter(aData)) {
      return NS_OK;
    }
    if (0 == strcmp(aTopic, mozilla::DllServices::kTopicDllLoadedMainThread)) {
      ++mMainThreadNotificationsCount;
    } else {
      EXPECT_TRUE(
          0 ==
          strcmp(aTopic, mozilla::DllServices::kTopicDllLoadedNonMainThread));
      ++mNonMainThreadNotificationsCount;
    }
    return NS_OK;
  }

  void Init() {
    nsCOMPtr<nsIObserverService> obsServ(
        mozilla::services::GetObserverService());

    EXPECT_TRUE(obsServ);

    obsServ->AddObserver(this, mozilla::DllServices::kTopicDllLoadedMainThread,
                         false);
    obsServ->AddObserver(
        this, mozilla::DllServices::kTopicDllLoadedNonMainThread, false);
  }

  void Exit() {
    // Observe only gets called if/when we flush the main thread loop.
    FlushMainThreadLoop();

    nsCOMPtr<nsIObserverService> obsServ(
        mozilla::services::GetObserverService());

    EXPECT_TRUE(obsServ);

    obsServ->RemoveObserver(this,
                            mozilla::DllServices::kTopicDllLoadedMainThread);
    obsServ->RemoveObserver(this,
                            mozilla::DllServices::kTopicDllLoadedNonMainThread);
  }

  int MainThreadNotificationsCount() { return mMainThreadNotificationsCount; }

  int NonMainThreadNotificationsCount() {
    return mNonMainThreadNotificationsCount;
  }

 private:
  virtual ~TestDLLLoadObserver() = default;

  DLLFilter mDllFilter;
  int mMainThreadNotificationsCount;
  int mNonMainThreadNotificationsCount;
};

NS_IMPL_ISUPPORTS(TestDLLLoadObserver, nsIObserver)

TEST(TestDllBlocklist, BlockDllByName)
{
  // The DLL name has capital letters, so this also tests that the comparison
  // is case-insensitive.
  constexpr auto kLeafName = u"TestDllBlocklist_MatchByName.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));

  hDll.own(::LoadLibraryExW(dllPath.get(), nullptr, LOAD_LIBRARY_AS_DATAFILE));
  // Mapped as MEM_MAPPED + PAGE_READONLY
  EXPECT_TRUE(hDll);
}

TEST(TestDllBlocklist, BlockDllByVersion)
{
  constexpr auto kLeafName = u"TestDllBlocklist_MatchByVersion.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));

  hDll.own(
      ::LoadLibraryExW(dllPath.get(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE));
  // Mapped as MEM_IMAGE + PAGE_READONLY
  EXPECT_TRUE(hDll);
}

TEST(TestDllBlocklist, AllowDllByVersion)
{
  constexpr auto kLeafName = u"TestDllBlocklist_AllowByVersion.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
}

TEST(TestDllBlocklist, GPUProcessOnly_AllowInMainProcess)
{
  constexpr auto kLeafName = u"TestDllBlocklist_GPUProcessOnly.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
}

TEST(TestDllBlocklist, SocketProcessOnly_AllowInMainProcess)
{
  constexpr auto kLeafName = u"TestDllBlocklist_SocketProcessOnly.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
}

TEST(TestDllBlocklist, UtilityProcessOnly_AllowInMainProcess)
{
  constexpr auto kLeafName = u"TestDllBlocklist_UtilityProcessOnly.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
}

TEST(TestDllBlocklist, GMPluginProcessOnly_AllowInMainProcess)
{
  constexpr auto kLeafName = u"TestDllBlocklist_GMPluginProcessOnly.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
}

// This DLL has two entries; it's blocked for unversioned (i.e. DLLs that
// have no version information) everywhere and blocked for versions 5.5.5.5 and
// earlier only in the GPU process. Since the version we're trying to load
// is 5.5.5.5, it should load in the main process.
TEST(TestDllBlocklist, MultipleEntriesDifferentProcesses_AllowInMainProcess)
{
  constexpr auto kLeafName =
      u"TestDllBlocklist_MultipleEntries_DifferentProcesses.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
}

TEST(TestDllBlocklist, MultipleEntriesSameProcessBackward_Block)
{
  // One entry matches by version and many others do not, so
  // we should block.
  constexpr auto kLeafName =
      u"TestDllBlocklist_MultipleEntries_SameProcessBackward.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));

  hDll.own(
      ::LoadLibraryExW(dllPath.get(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE));
  // Mapped as MEM_IMAGE + PAGE_READONLY
  EXPECT_TRUE(hDll);
}

TEST(TestDllBlocklist, MultipleEntriesSameProcessForward_Block)
{
  // One entry matches by version and many others do not, so
  // we should block.
  constexpr auto kLeafName =
      u"TestDllBlocklist_MultipleEntries_SameProcessForward.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));

  hDll.own(
      ::LoadLibraryExW(dllPath.get(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE));
  // Mapped as MEM_IMAGE + PAGE_READONLY
  EXPECT_TRUE(hDll);
}

#if defined(MOZ_LAUNCHER_PROCESS)
// RedirectToNoOpEntryPoint needs the launcher process.
// This test will fail in debug x64 if we mistakenly reintroduce stack buffers
// in patched_NtMapViewOfSection (see bug 1733532).
TEST(TestDllBlocklist, NoOpEntryPoint)
{
  // DllMain of this dll has MOZ_RELEASE_ASSERT.  This test makes sure we load
  // the module successfully without running DllMain.
  constexpr auto kLeafName = u"TestDllBlocklist_NoOpEntryPoint.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

#  if defined(MOZ_ASAN)
  // With ASAN, the test uses mozglue's blocklist where
  // REDIRECT_TO_NOOP_ENTRYPOINT is ignored.  So LoadLibraryW
  // is expected to fail.
  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));
#  else
  EXPECT_TRUE(!!hDll);
  EXPECT_TRUE(!!::GetModuleHandleW(kLeafName.get()));
#  endif
}

// User blocklist needs the launcher process.
// This test will fail in debug x64 if we mistakenly reintroduce stack buffers
// in patched_NtMapViewOfSection (see bug 1733532).
TEST(TestDllBlocklist, UserBlocked)
{
  constexpr auto kLeafName = u"TestDllBlocklist_UserBlocked.dll"_ns;
  nsString dllPath = GetFullPath(kLeafName);

  nsModuleHandle hDll(::LoadLibraryW(dllPath.get()));

// With ASAN, the test uses mozglue's blocklist where
// the user blocklist is not used.
#  if !defined(MOZ_ASAN)
  EXPECT_TRUE(!hDll);
  EXPECT_TRUE(!::GetModuleHandleW(kLeafName.get()));
#  endif
  hDll.own(::LoadLibraryExW(dllPath.get(), nullptr, LOAD_LIBRARY_AS_DATAFILE));
  // Mapped as MEM_MAPPED + PAGE_READONLY
  EXPECT_TRUE(hDll);
}
#endif  // defined(MOZ_LAUNCHER_PROCESS)

#define DLL_BLOCKLIST_ENTRY(name, ...) {name, __VA_ARGS__},
#define DLL_BLOCKLIST_STRING_TYPE const char*
#include "mozilla/WindowsDllBlocklistLegacyDefs.h"

TEST(TestDllBlocklist, BlocklistIntegrity)
{
  DECLARE_POINTER_TO_FIRST_DLL_BLOCKLIST_ENTRY(pFirst);
  DECLARE_POINTER_TO_LAST_DLL_BLOCKLIST_ENTRY(pLast);

  EXPECT_FALSE(pLast->mName || pLast->mMaxVersion || pLast->mFlags);

  for (size_t i = 0; i < mozilla::ArrayLength(gWindowsDllBlocklist) - 1; ++i) {
    auto pEntry = pFirst + i;

    // Validate name
    EXPECT_TRUE(!!pEntry->mName);
    EXPECT_GT(strlen(pEntry->mName), 3U);

    // Check the filename for valid characters.
    for (auto pch = pEntry->mName; *pch != 0; ++pch) {
      EXPECT_FALSE(*pch >= 'A' && *pch <= 'Z');
    }
  }
}

TEST(TestDllBlocklist, BlockThreadWithLoadLibraryEntryPoint)
{
  // Only supported on Nightly
#if defined(NIGHTLY_BUILD)
  using ThreadProc = unsigned(__stdcall*)(void*);

  constexpr auto kLeafNameW = u"TestDllBlocklist_MatchByVersion.dll"_ns;

  nsString fullPathW = GetFullPath(kLeafNameW);
  EXPECT_FALSE(fullPathW.IsEmpty());

  nsAutoHandle threadW(reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr, 0, reinterpret_cast<ThreadProc>(&::LoadLibraryW),
                     (void*)fullPathW.get(), 0, nullptr)));

  EXPECT_TRUE(!!threadW);
  EXPECT_EQ(::WaitForSingleObject(threadW, INFINITE), WAIT_OBJECT_0);

#  if !defined(MOZ_ASAN)
  // ASAN builds under Windows 11 can have unexpected thread exit codes.
  // See bug 1798796
  DWORD exitCode;
  EXPECT_TRUE(::GetExitCodeThread(threadW, &exitCode) && !exitCode);
#  endif  // !defined(MOZ_ASAN)
  EXPECT_TRUE(!::GetModuleHandleW(kLeafNameW.get()));

  const NS_LossyConvertUTF16toASCII fullPathA(fullPathW);
  EXPECT_FALSE(fullPathA.IsEmpty());

  nsAutoHandle threadA(reinterpret_cast<HANDLE>(
      _beginthreadex(nullptr, 0, reinterpret_cast<ThreadProc>(&::LoadLibraryA),
                     (void*)fullPathA.get(), 0, nullptr)));

  EXPECT_TRUE(!!threadA);
  EXPECT_EQ(::WaitForSingleObject(threadA, INFINITE), WAIT_OBJECT_0);
#  if !defined(MOZ_ASAN)
  // ASAN builds under Windows 11 can have unexpected thread exit codes.
  // See bug 1798796
  EXPECT_TRUE(::GetExitCodeThread(threadA, &exitCode) && !exitCode);
#  endif  // !defined(MOZ_ASAN)
  EXPECT_TRUE(!::GetModuleHandleW(kLeafNameW.get()));
#endif  // defined(NIGHTLY_BUILD)
}

constexpr auto kSingleNotificationDll1Loads = 4;
constexpr auto kSingleNotificationDll2Loads = 3;
using DllFullPathsArray =
    std::array<nsString,
               kSingleNotificationDll1Loads + kSingleNotificationDll2Loads>;

DWORD __stdcall LoadSingleNotificationModules(LPVOID aThreadParameter) {
  auto* dllFullPaths = reinterpret_cast<DllFullPathsArray*>(aThreadParameter);

  for (const auto& dllFullPath : *dllFullPaths) {
    nsModuleHandle hDll(::LoadLibraryW(dllFullPath.get()));
    EXPECT_TRUE(!hDll);
    EXPECT_TRUE(!::GetModuleHandleW(dllFullPath.get()));
  }

  return 0;
}

// The next test is only relevant if we hook LdrLoadDll, so we reflect the
// hooking condition from browser/app/winlauncher/DllBlocklistInit.cpp.
#if !defined(MOZ_ASAN) && !defined(_M_ARM64)

// This test relies on the fact that blocked DLL loads generate a DLL load
// notification.
TEST(TestDllBlocklist, SingleNotification)
{
  // We will block-load the two DLLs multiple times, with variations on case.
  std::array<nsLiteralString, kSingleNotificationDll1Loads> dll1Variations{
      u"TestDllBlocklist_SingleNotification1.dll"_ns,
      u"TestDllBlocklist_SingleNotification1.dll"_ns,
      u"testdllblocklist_singlenotification1.dll"_ns,
      u"TeStDlLbLoCkLiSt_SiNgLeNoTiFiCaTiOn1.DlL"_ns,
  };
  std::array<nsLiteralString, kSingleNotificationDll2Loads> dll2Variations{
      u"TestDllBlocklist_SingleNotification2.dll"_ns,
      u"testdllblocklist_singlenotification2.dll"_ns,
      u"TESTDLLBLOCKLIST_SINGLENOTIFICATION2.dll"_ns,
  };
  DllFullPathsArray dllFullPaths;
  size_t i = 0;
  for (const auto& dllName : dll1Variations) {
    dllFullPaths[i] = GetFullPath(dllName);
    ++i;
  }
  for (const auto& dllName : dll2Variations) {
    dllFullPaths[i] = GetFullPath(dllName);
    ++i;
  }

  // Register our observer.
  TestDLLLoadObserver::DLLFilter dllFilter(
      [](const char16_t* aLoadedPath) -> bool {
        nsDependentString loadedPath(aLoadedPath);
        return StringEndsWith(loadedPath,
                              u"\\testdllblocklist_singlenotification1.dll"_ns,
                              nsCaseInsensitiveStringComparator) ||
               StringEndsWith(loadedPath,
                              u"\\testdllblocklist_singlenotification2.dll"_ns,
                              nsCaseInsensitiveStringComparator);
      });
  RefPtr<TestDLLLoadObserver> obs(new TestDLLLoadObserver(dllFilter));
  obs->Init();

  // Load DllServices. This is required for notifications to get dispatched.
  RefPtr<mozilla::DllServices> dllSvc(mozilla::DllServices::Get());
  EXPECT_TRUE(dllSvc);

  // Block-load the two DLLs multiple times on the main thread.
  LoadSingleNotificationModules(reinterpret_cast<void*>(&dllFullPaths));

  // Block-load the two DLLs multiple times on a different thread.
  HANDLE thread =
      ::CreateThread(nullptr, 0, LoadSingleNotificationModules,
                     reinterpret_cast<void*>(&dllFullPaths), 0, nullptr);
  EXPECT_EQ(::WaitForSingleObject(thread, 5000), WAIT_OBJECT_0);

  // Unregister our observer and flush the main thread loop.
  obs->Exit();

  // Check how many notifications we received
  EXPECT_EQ(obs->MainThreadNotificationsCount(), 2);
  EXPECT_EQ(obs->NonMainThreadNotificationsCount(),
            kSingleNotificationDll1Loads + kSingleNotificationDll2Loads);
}

#endif  // !defined(MOZ_ASAN) && !defined(_M_ARM64)

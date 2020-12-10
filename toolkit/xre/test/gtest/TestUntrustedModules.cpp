/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"

#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/UntrustedModulesProcessor.h"
#include "mozilla/WinDllServices.h"
#include "nsDirectoryServiceDefs.h"
#include "TelemetryFixture.h"

class ModuleLoadCounter final {
  nsDataHashtable<nsStringCaseInsensitiveHashKey, int> mCounters;

 public:
  template <int N>
  ModuleLoadCounter(const nsString (&aNames)[N], const int (&aCounts)[N])
      : mCounters(N) {
    for (int i = 0; i < N; ++i) {
      mCounters.Put(aNames[i], aCounts[i]);
    }
  }

  template <int N>
  bool Remains(const nsString (&aNames)[N], const int (&aCounts)[N]) {
    EXPECT_EQ(mCounters.Count(), N);
    if (mCounters.Count() != N) {
      return false;
    }

    bool result = true;
    for (int i = 0; i < N; ++i) {
      int* entry = mCounters.GetValue(aNames[i]);
      if (!entry) {
        wprintf(L"%s is not registered.\n", aNames[i].get());
        result = false;
      } else if (*entry != aCounts[i]) {
        // We can return false, but let's print out all unmet modules
        // which may be helpful to investigate test failures.
        wprintf(L"%s:%4d\n", aNames[i].get(), *entry);
        result = false;
      }
    }
    return result;
  }

  bool IsDone() const {
    bool allZero = true;
    for (auto iter = mCounters.ConstIter(); !iter.Done(); iter.Next()) {
      if (iter.Data() < 0) {
        // If any counter is negative, we know the test fails.
        // No need to continue.
        return true;
      }
      if (iter.Data() > 0) {
        allZero = false;
      }
    }
    // If all counters are zero, the test finished nicely.  Otherwise, those
    // counters are expected to be decremented later.  Let's continue.
    return allZero;
  }

  void Decrement(const nsString& aName) {
    if (int* entry = mCounters.GetValue(aName)) {
      --(*entry);
    }
  }
};

class UntrustedModulesCollector {
  static constexpr int kMaximumPendingQueries = 200;
  Vector<UntrustedModulesData> mData;

 public:
  const Vector<UntrustedModulesData>& Data() const { return mData; }

  nsresult Collect(ModuleLoadCounter& aChecker) {
    nsresult rv = NS_OK;

    mData.clear();
    int pendingQueries = 0;

    EXPECT_TRUE(SpinEventLoopUntil([this, &pendingQueries, &aChecker, &rv]() {
      // Some of expected loaded modules are still missing
      // after kMaximumPendingQueries queries were submitted.
      // Giving up here to avoid an infinite loop.
      if (pendingQueries >= kMaximumPendingQueries) {
        rv = NS_ERROR_ABORT;
        return true;
      }

      ++pendingQueries;

      RefPtr<DllServices> dllSvc(DllServices::Get());
      dllSvc->GetUntrustedModulesData()->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [this, &pendingQueries,
           &aChecker](Maybe<UntrustedModulesData>&& aResult) {
            EXPECT_GT(pendingQueries, 0);
            --pendingQueries;

            if (aResult.isSome()) {
              wprintf(L"Received data. (pendingQueries=%d)\n", pendingQueries);
              for (const auto& evt : aResult.ref().mEvents) {
                aChecker.Decrement(evt.mRequestedDllName);
              }
              EXPECT_TRUE(mData.emplaceBack(std::move(aResult.ref())));
            }
          },
          [&pendingQueries, &rv](nsresult aReason) {
            EXPECT_GT(pendingQueries, 0);
            --pendingQueries;

            wprintf(L"GetUntrustedModulesData() failed - %08x\n", aReason);
            EXPECT_TRUE(false);
            rv = aReason;
          });

      // Keep calling GetUntrustedModulesData() until we meet the condition.
      return aChecker.IsDone();
    }));

    EXPECT_TRUE(SpinEventLoopUntil(
        [&pendingQueries]() { return pendingQueries <= 0; }));

    return rv;
  }
};

static void ValidateUntrustedModules(const UntrustedModulesData& aData) {
  EXPECT_EQ(aData.mProcessType, GeckoProcessType_Default);
  EXPECT_EQ(aData.mPid, ::GetCurrentProcessId());

  nsTHashtable<nsPtrHashKey<void>> moduleSet;
  for (auto iter = aData.mModules.ConstIter(); !iter.Done(); iter.Next()) {
    const RefPtr<ModuleRecord>& module = iter.Data();
    moduleSet.PutEntry(module);
  }

  for (const auto& evt : aData.mEvents) {
    EXPECT_EQ(evt.mThreadId, ::GetCurrentThreadId());
    // Make sure mModule is pointing to an entry of mModules.
    EXPECT_TRUE(moduleSet.Contains(evt.mModule));
    EXPECT_FALSE(evt.mIsDependent);
  }

  EXPECT_GT(aData.mEvents.length(), 0);
  EXPECT_GT(aData.mStacks.GetModuleCount(), 0);
  // Because xul.dll is not signed when running GTest,
  // mXULLoadDurationMS is expected to be empty.
  EXPECT_TRUE(aData.mXULLoadDurationMS.isNothing());
  EXPECT_EQ(aData.mSanitizationFailures, 0);
  EXPECT_EQ(aData.mTrustTestFailures, 0);
}

class UntrustedModulesFixture : public TelemetryTestFixture {
  static constexpr int kLoadCountBeforeDllServices = 5;
  static constexpr int kLoadCountAfterDllServices = 5;

  // One of the important test scenarios is to load modules before DllServices
  // is initialized and to make sure those loading events are forwarded when
  // DllServices is initialized.
  // However, GTest instantiates a Fixture class every testcase and there is
  // no way to re-enable DllServices and UntrustedModulesProcessor once it's
  // disabled, which means no matter how many testcases we have, only the
  // first testcase exercises that scenario.  That's why we implement that
  // test scenario in InitialModuleLoadOnce as a static member and runs it
  // in the first testcase to be executed.
  static INIT_ONCE sInitLoadOnce;
  static UntrustedModulesCollector sInitLoadDataCollector;

  static nsString PrependWorkingDir(const nsAString& aLeaf) {
    nsCOMPtr<nsIFile> file;
    EXPECT_TRUE(NS_SUCCEEDED(NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR,
                                                    getter_AddRefs(file))));
    EXPECT_TRUE(NS_SUCCEEDED(file->Append(aLeaf)));
    bool exists;
    EXPECT_TRUE(NS_SUCCEEDED(file->Exists(&exists)) && exists);
    nsString fullPath;
    EXPECT_TRUE(NS_SUCCEEDED(file->GetPath(fullPath)));
    return fullPath;
  }

  static BOOL CALLBACK InitialModuleLoadOnce(PINIT_ONCE, void*, void**);

 protected:
  static constexpr int kInitLoadCount =
      kLoadCountBeforeDllServices + kLoadCountAfterDllServices;
  static const nsString kTestModules[];

  static void LoadAndFree(const nsAString& aLeaf) {
    nsModuleHandle dll(::LoadLibraryW(PrependWorkingDir(aLeaf).get()));
    EXPECT_TRUE(!!dll);
  }

  virtual void SetUp() override {
    TelemetryTestFixture::SetUp();
    ::InitOnceExecuteOnce(&sInitLoadOnce, InitialModuleLoadOnce, nullptr,
                          nullptr);
  }

  static const Vector<UntrustedModulesData>& GetInitLoadData() {
    return sInitLoadDataCollector.Data();
  }
};

const nsString UntrustedModulesFixture::kTestModules[] = {
    u"TestUntrustedModules_Dll1.dll"_ns, u"TestUntrustedModules_Dll2.dll"_ns};
INIT_ONCE UntrustedModulesFixture::sInitLoadOnce = INIT_ONCE_STATIC_INIT;
UntrustedModulesCollector UntrustedModulesFixture::sInitLoadDataCollector;

BOOL CALLBACK UntrustedModulesFixture::InitialModuleLoadOnce(PINIT_ONCE, void*,
                                                             void**) {
  for (int i = 0; i < kLoadCountBeforeDllServices; ++i) {
    for (const auto& mod : kTestModules) {
      LoadAndFree(mod);
    }
  }

  RefPtr<DllServices> dllSvc(DllServices::Get());
  dllSvc->StartUntrustedModulesProcessor();

  for (int i = 0; i < kLoadCountAfterDllServices; ++i) {
    for (const auto& mod : kTestModules) {
      LoadAndFree(mod);
    }
  }

  ModuleLoadCounter waitForTwo(kTestModules, {kInitLoadCount, kInitLoadCount});
  EXPECT_EQ(sInitLoadDataCollector.Collect(waitForTwo), NS_OK);
  EXPECT_TRUE(waitForTwo.Remains(kTestModules, {0, 0}));

  for (const auto& event : GetInitLoadData()) {
    ValidateUntrustedModules(event);
  }

  // Data was removed when retrieved.  No data is retrieved again.
  UntrustedModulesCollector collector;
  ModuleLoadCounter waitOnceForEach(kTestModules, {1, 1});
  EXPECT_EQ(collector.Collect(waitOnceForEach), NS_ERROR_ABORT);
  EXPECT_TRUE(waitOnceForEach.Remains(kTestModules, {1, 1}));

  return TRUE;
}

TEST_F(UntrustedModulesFixture, Main) {}

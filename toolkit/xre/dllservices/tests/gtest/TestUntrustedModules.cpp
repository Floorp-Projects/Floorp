/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"

#include "js/RegExp.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/UntrustedModulesProcessor.h"
#include "mozilla/WinDllServices.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "TelemetryFixture.h"
#include "UntrustedModulesBackupService.h"
#include "UntrustedModulesDataSerializer.h"

using namespace mozilla;

class ModuleLoadCounter final {
  nsTHashMap<nsStringCaseInsensitiveHashKey, int> mCounters;

 public:
  template <size_t N>
  ModuleLoadCounter(const nsString (&aNames)[N], const int (&aCounts)[N])
      : mCounters(N) {
    for (size_t i = 0; i < N; ++i) {
      mCounters.InsertOrUpdate(aNames[i], aCounts[i]);
    }
  }

  template <size_t N>
  bool Remains(const nsString (&aNames)[N], const int (&aCounts)[N]) {
    EXPECT_EQ(mCounters.Count(), N);
    if (mCounters.Count() != N) {
      return false;
    }

    bool result = true;
    for (size_t i = 0; i < N; ++i) {
      auto entry = mCounters.Lookup(aNames[i]);
      if (!entry) {
        wprintf(L"%s is not registered.\n",
                static_cast<const wchar_t*>(aNames[i].get()));
        result = false;
      } else if (*entry != aCounts[i]) {
        // We can return false, but let's print out all unmet modules
        // which may be helpful to investigate test failures.
        wprintf(L"%s:%4d\n", static_cast<const wchar_t*>(aNames[i].get()),
                *entry);
        result = false;
      }
    }
    return result;
  }

  bool IsDone() const {
    bool allZero = true;
    for (const auto& data : mCounters.Values()) {
      if (data < 0) {
        // If any counter is negative, we know the test fails.
        // No need to continue.
        return true;
      }
      if (data > 0) {
        allZero = false;
      }
    }
    // If all counters are zero, the test finished nicely.  Otherwise, those
    // counters are expected to be decremented later.  Let's continue.
    return allZero;
  }

  void Decrement(const nsString& aName) {
    if (auto entry = mCounters.Lookup(aName)) {
      --(*entry);
    }
  }
};

class UntrustedModulesCollector {
  static constexpr int kMaximumPendingQueries = 500;
  Vector<UntrustedModulesData> mData;

 public:
  Vector<UntrustedModulesData>& Data() { return mData; }

  nsresult Collect(ModuleLoadCounter& aChecker) {
    nsresult rv = NS_OK;

    mData.clear();
    int pendingQueries = 0;

    EXPECT_TRUE(SpinEventLoopUntil(
        "xre:UntrustedModulesCollector"_ns,
        [this, &pendingQueries, &aChecker, &rv]() {
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
                  wprintf(L"Received data. (pendingQueries=%d)\n",
                          pendingQueries);
                  for (auto item : aResult.ref().mEvents) {
                    aChecker.Decrement(item->mEvent.mRequestedDllName);
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
        "xre:UntrustedModulesCollector(pendingQueries)"_ns,
        [&pendingQueries]() { return pendingQueries <= 0; }));

    return rv;
  }
};

class UntrustedModulesFixture : public TelemetryTestFixture {
  static constexpr int kLoadCountBeforeDllServices = 5;
  static constexpr int kLoadCountAfterDllServices = 5;
  static constexpr uint32_t kMaxModulesArrayLen = 10;

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

  static void ValidateUntrustedModules(const UntrustedModulesData& aData,
                                       bool aIsTruncatedData = false);

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

  // This method is useful if we want a new instance of UntrustedModulesData
  // which is not copyable.
  static UntrustedModulesData CollectSingleData() {
    // If we call LoadAndFree more than once, those loading events are
    // likely to be merged into an instance of UntrustedModulesData,
    // meaning the length of the collector's vector is at least one but
    // the exact number is unknown.
    LoadAndFree(kTestModules[0]);

    UntrustedModulesCollector collector;
    ModuleLoadCounter waitForOne({kTestModules[0]}, {1});
    EXPECT_TRUE(NS_SUCCEEDED(collector.Collect(waitForOne)));
    EXPECT_TRUE(waitForOne.Remains({kTestModules[0]}, {0}));
    EXPECT_EQ(collector.Data().length(), 1U);

    // Cannot "return collector.Data()[0]" as copy ctor is deleted.
    return UntrustedModulesData(std::move(collector.Data()[0]));
  }

  template <typename DataFetcherT>
  void ValidateJSValue(const char16_t* aPattern, size_t aPatternLength,
                       DataFetcherT&& aDataFetcher) {
    AutoJSContextWithGlobal cx(mCleanGlobal);
    mozilla::Telemetry::UntrustedModulesDataSerializer serializer(
        cx.GetJSContext(), kMaxModulesArrayLen);
    EXPECT_TRUE(!!serializer);
    aDataFetcher(serializer);

    JS::Rooted<JS::Value> jsval(cx.GetJSContext());
    serializer.GetObject(&jsval);

    nsAutoString json;
    EXPECT_TRUE(nsContentUtils::StringifyJSON(cx.GetJSContext(), &jsval, json));

    JS::Rooted<JSObject*> re(
        cx.GetJSContext(),
        JS::NewUCRegExpObject(cx.GetJSContext(), aPattern, aPatternLength,
                              JS::RegExpFlag::Global));
    EXPECT_TRUE(!!re);

    JS::Rooted<JS::Value> matchResult(cx.GetJSContext(), JS::NullValue());
    size_t idx = 0;
    EXPECT_TRUE(JS::ExecuteRegExpNoStatics(cx.GetJSContext(), re, json.get(),
                                           json.Length(), &idx, true,
                                           &matchResult));
    // On match, with aOnlyMatch = true, ExecuteRegExpNoStatics returns boolean
    // true.  If no match, ExecuteRegExpNoStatics returns Null.
    EXPECT_TRUE(matchResult.isBoolean() && matchResult.toBoolean());
    if (!matchResult.isBoolean() || !matchResult.toBoolean()) {
      // If match failed, print out the actual JSON kindly.
      wprintf(L"JSON: %s\n", static_cast<const wchar_t*>(json.get()));
      wprintf(L"RE: %s\n", aPattern);
    }
  }
};

const nsString UntrustedModulesFixture::kTestModules[] = {
    // Sorted for binary-search
    u"TestUntrustedModules_Dll1.dll"_ns,
    u"TestUntrustedModules_Dll2.dll"_ns,
};

INIT_ONCE UntrustedModulesFixture::sInitLoadOnce = INIT_ONCE_STATIC_INIT;
UntrustedModulesCollector UntrustedModulesFixture::sInitLoadDataCollector;

void UntrustedModulesFixture::ValidateUntrustedModules(
    const UntrustedModulesData& aData, bool aIsTruncatedData) {
  // This defines a list of modules which are listed on our blocklist and
  // thus its loading status is not expected to be Status::Loaded.
  // Although the UntrustedModulesFixture test does not touch any of them,
  // the current process might have run a test like TestDllBlocklist where
  // we try to load and block them.
  const struct {
    const wchar_t* mName;
    ModuleLoadInfo::Status mStatus;
  } kKnownModules[] = {
      // Sorted by mName for binary-search
      {L"TestDllBlocklist_MatchByName.dll", ModuleLoadInfo::Status::Blocked},
      {L"TestDllBlocklist_MatchByVersion.dll", ModuleLoadInfo::Status::Blocked},
      {L"TestDllBlocklist_NoOpEntryPoint.dll",
       ModuleLoadInfo::Status::Redirected},
  };

  EXPECT_EQ(aData.mProcessType, GeckoProcessType_Default);
  EXPECT_EQ(aData.mPid, ::GetCurrentProcessId());

  nsTHashtable<nsPtrHashKey<void>> moduleSet;
  for (const RefPtr<ModuleRecord>& module : aData.mModules.Values()) {
    moduleSet.PutEntry(module);
  }

  size_t numBlockedEvents = 0;
  for (auto item : aData.mEvents) {
    const auto& evt = item->mEvent;
    const nsDependentSubstring leafName =
        nt::GetLeafName(evt.mModule->mResolvedNtName);
    const nsAutoString leafNameStr(leafName.Data(), leafName.Length());
    const ModuleLoadInfo::Status loadStatus =
        static_cast<ModuleLoadInfo::Status>(evt.mLoadStatus);
    if (loadStatus == ModuleLoadInfo::Status::Blocked) {
      ++numBlockedEvents;
    }

    size_t match;
    if (BinarySearchIf(
            kKnownModules, 0, ArrayLength(kKnownModules),
            [&leafNameStr](const auto& aVal) {
              return _wcsicmp(leafNameStr.get(), aVal.mName);
            },
            &match)) {
      EXPECT_EQ(loadStatus, kKnownModules[match].mStatus);
    } else {
      EXPECT_EQ(evt.mLoadStatus, 0U);
    }

    if (BinarySearchIf(
            kTestModules, 0, ArrayLength(kTestModules),
            [&leafNameStr](const auto& aVal) {
              return _wcsicmp(leafNameStr.get(), aVal.get());
            },
            &match)) {
      // We know the test modules are loaded in the main thread,
      // but we don't know about other modules.
      EXPECT_EQ(evt.mThreadId, ::GetCurrentThreadId());
    }

    // Make sure mModule is pointing to an entry of mModules.
    EXPECT_TRUE(moduleSet.Contains(evt.mModule));
    EXPECT_FALSE(evt.mIsDependent);
  }

  // No check for the mXULLoadDurationMS field because the field has a value
  // in CCov build GTest, but it is empty in non-CCov build (bug 1681936).
  EXPECT_EQ(aData.mNumEvents, aData.mEvents.length());
  EXPECT_GT(aData.mNumEvents, 0U);
  if (aIsTruncatedData) {
    EXPECT_EQ(aData.mStacks.GetModuleCount(), 0U);
    EXPECT_LE(aData.mNumEvents, UntrustedModulesData::kMaxEvents);
  } else if (numBlockedEvents == aData.mNumEvents) {
    // If all loading events were blocked or aData is truncated,
    // the stacks are empty.
    EXPECT_EQ(aData.mStacks.GetModuleCount(), 0U);
  } else {
    EXPECT_GT(aData.mStacks.GetModuleCount(), 0U);
  }
  EXPECT_EQ(aData.mSanitizationFailures, 0U);
  EXPECT_EQ(aData.mTrustTestFailures, 0U);
}

BOOL CALLBACK UntrustedModulesFixture::InitialModuleLoadOnce(PINIT_ONCE, void*,
                                                             void**) {
  for (int i = 0; i < kLoadCountBeforeDllServices; ++i) {
    for (const auto& mod : kTestModules) {
      LoadAndFree(mod);
    }
  }

  RefPtr<DllServices> dllSvc(DllServices::Get());
  dllSvc->StartUntrustedModulesProcessor(true);

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

#define PROCESS_OBJ(TYPE, PID) \
  u"\"" TYPE u"\\." PID u"\":{" \
    u"\"processType\":\"" TYPE u"\",\"elapsed\":\\d+\\.\\d+," \
    u"\"sanitizationFailures\":0,\"trustTestFailures\":0," \
    u"\"events\":\\[{" \
      u"\"processUptimeMS\":\\d+,\"loadDurationMS\":\\d+\\.\\d+," \
      u"\"threadID\":\\d+,\"threadName\":\"Main Thread\"," \
      u"\"baseAddress\":\"0x[0-9a-f]+\",\"moduleIndex\":0," \
      u"\"isDependent\":false,\"loadStatus\":0}\\]," \
    u"\"combinedStacks\":{" \
      u"\"memoryMap\":\\[\\[\"\\w+\\.\\w+\",\"[0-9A-Z]+\"\\]" \
        u"(,\\[\"\\w+\\.\\w+\",\"[0-9A-Z]+\\\"\\])*\\]," \
      u"\"stacks\":\\[\\[\\[(-1|\\d+),\\d+\\]" \
        u"(,\\[(-1|\\d+),\\d+\\])*\\]\\]}}"

TEST_F(UntrustedModulesFixture, Serialize) {
  // clang-format off
  const char16_t kPattern[] = u"{\"structVersion\":1,"
    u"\"modules\":\\[{"
      u"\"resolvedDllName\":\"TestUntrustedModules_Dll1\\.dll\","
      u"\"fileVersion\":\"1\\.2\\.3\\.4\","
      u"\"companyName\":\"Mozilla Corporation\",\"trustFlags\":0}\\],"
    u"\"processes\":{"
      PROCESS_OBJ(u"browser", u"0xabc") u","
      PROCESS_OBJ(u"browser", u"0x4") u","
      PROCESS_OBJ(u"rdd", u"0x4")
  u"}}";
  // clang-format on

  UntrustedModulesBackupData backup1, backup2;
  {
    UntrustedModulesData data1 = CollectSingleData();
    UntrustedModulesData data2 = CollectSingleData();
    UntrustedModulesData data3 = CollectSingleData();

    data1.mPid = 0xabc;
    data2.mPid = 0x4;
    data2.mProcessType = GeckoProcessType_RDD;
    data3.mPid = 0x4;

    backup1.Add(std::move(data1));
    backup2.Add(std::move(data2));
    backup1.Add(std::move(data3));
  }

  ValidateJSValue(kPattern, ArrayLength(kPattern) - 1,
                  [&backup1, &backup2](
                      Telemetry::UntrustedModulesDataSerializer& aSerializer) {
                    EXPECT_TRUE(NS_SUCCEEDED(aSerializer.Add(backup1)));
                    EXPECT_TRUE(NS_SUCCEEDED(aSerializer.Add(backup2)));
                  });
}

TEST_F(UntrustedModulesFixture, Backup) {
  RefPtr<UntrustedModulesBackupService> backupSvc(
      UntrustedModulesBackupService::Get());
  for (int i = 0; i < 100; ++i) {
    backupSvc->Backup(CollectSingleData());
  }

  backupSvc->SettleAllStagingData();
  EXPECT_TRUE(backupSvc->Staging().IsEmpty());

  for (const auto& entry : backupSvc->Settled()) {
    const RefPtr<UntrustedModulesDataContainer>& container = entry.GetData();
    EXPECT_TRUE(!!container);
    const UntrustedModulesData& data = container->mData;
    EXPECT_EQ(entry.GetKey(), ProcessHashKey(data.mProcessType, data.mPid));
    ValidateUntrustedModules(data, /*aIsTruncatedData*/ true);
  }
}

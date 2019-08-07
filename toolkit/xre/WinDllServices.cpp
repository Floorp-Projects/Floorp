/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/WinDllServices.h"

#include <windows.h>
#include <psapi.h>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/HashTable.h"
#include "mozilla/mozalloc.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"
#include "mozilla/StaticLocalPtr.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsString.h"
#include "WinUtils.h"

namespace mozilla {

// Returns a Vector of currently-loaded module base addresses. Basically this
// is a wrapper around EnumProcessModulesEx()
// In case of error, returns an empty Vector.
static Vector<uintptr_t, 0, InfallibleAllocPolicy> GetProcessModuleBases() {
  Vector<uintptr_t, 0, InfallibleAllocPolicy> ret;
  // At the time this is called, we are far into process execution so we can
  // expect quite a few modules to be loaded. 100 seems reasonable to start.
  static const int kProcessModulesInitialCapacity = 100;
  Unused << ret.resize(kProcessModulesInitialCapacity);
  DWORD cbNeeded = 0;
  while (true) {
    if (!EnumProcessModulesEx(GetCurrentProcess(), (HMODULE*)ret.begin(),
                              ret.length() * sizeof(uintptr_t), &cbNeeded,
                              LIST_MODULES_ALL)) {
      // If it fails, return empty. There's no way to guarantee the partial
      // data is still good.
      return Vector<uintptr_t, 0, InfallibleAllocPolicy>();
    }
    size_t elementsNeeded = cbNeeded / sizeof(HMODULE);
    if (elementsNeeded <= ret.length()) {
      // Success; resize to the real number of elements.
      Unused << ret.resize(elementsNeeded);
      return ret;
    }
    // Increase the size of ret and try again.
    Unused << ret.resize(elementsNeeded);
  }
}

// This class keeps track of incoming module load events, and takes
// care of processing these events, weeding out trusted DLLs and filling in
// remaining data.
class UntrustedModulesManager {
  // This mutex does synchronization for all members.
  //
  // WARNING: This mutex locks during the Windows loader, which means you must
  // never invoke the loader from within this lock, even if you're locking from
  // outside the loader.
  Mutex mMutex;

  // We want to only process startup modules once, so keep track of that here.
  bool mHasProcessedStartupModules = false;

  ModuleEvaluator mEvaluator;
  int mErrorModules = 0;
  Maybe<double> mXULLoadDurationMS;

  // In order to get a list of modules loaded at startup, we take a list of
  // currently-loaded modules, and subtract:
  // - Modules in mProcessedEvents, which have been considered untrusted,
  // - Modules in mTrustedModuleHistory, which have been seen but discarded
  // The resulting list is a list of modules that we haven't seen before at all
  // and are (likely) still loaded.
  //
  // We can get away with comparing only base addresses (instead of full path),
  // because we're specifically searching for DLLs loaded at startup and remain
  // loaded.
  HashSet<uintptr_t, DefaultHasher<uintptr_t>, InfallibleAllocPolicy>
      mTrustedModuleHistory;

  // Incoming events, queued for processing (not yet evaluated)
  Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy> mQueuedEvents;

  // Items that have been processed, considered untrusted, and ready for
  // telemetry consumption.
  //
  // Note that the contents of mProcessedEvents and mProcessedStacks must
  // always remain in sync, element-for-element.
  Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy> mProcessedEvents;
  Telemetry::CombinedStacks mProcessedStacks;

 public:
  UntrustedModulesManager() : mMutex("UntrustedModulesManager::mMutex") {
    // Ensure whitelisted paths are initialized on the main thread.
    MOZ_ASSERT(NS_IsMainThread());
    widget::WinUtils::GetWhitelistedPaths();
  }

  // Handles incoming loader events, places events into the queue for later
  // processing.
  //
  // WARNING: This is called within the loader; only trivial calls are allowed.
  void OnNewEvents(
      const Vector<glue::ModuleLoadEvent, 0, InfallibleAllocPolicy>& aEvents) {
    // Because we can only get the thread name from the current thread, this
    // is the last chance to fill in thread name.
    const char* thisThreadName = PR_GetThreadName(PR_GetCurrentThread());

    // Lock mQueuedEvents to append events and fill in thread name if
    // possible... Only trivial (loader lock friendly) code allowed here!
    MutexAutoLock lock(mMutex);

    for (auto& event : aEvents) {
      Unused << mQueuedEvents.emplaceBack(ModuleLoadEvent(event));
      if (thisThreadName && (event.mThreadID == ::GetCurrentThreadId())) {
        mQueuedEvents.back().mThreadName = thisThreadName;
      }
    }
  }

  /**
   * Run from a worker thread, this will process and move items from the
   * mQueuedEvents into mProcessedEvents and mProcessedStacks
   * @param aHasProcessedStartupModules [out] Receives the value of
   *          mHasProcessedStartupModules. We grab this value during a lock, and
   *          the caller will need it for subsequent calls, so passing it around
   *          like this avoids at least one lock. The only risk with this is
   *          that we could end up calling ProcessStartupModules() multiple
   *          times, which is totally safe, and would be extremely rare.
   */
  void ProcessQueuedEvents(bool& aHasProcessedStartupModules) {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(!!mEvaluator);

    // Hold a reference to DllServices to ensure the object doesn't get deleted
    // during this call.
    RefPtr<DllServices> dllSvcRef(DllServices::Get());
    if (!dllSvcRef) {
      return;
    }

    Telemetry::BatchProcessedStackGenerator stackProcessor;

    Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy> queuedEvents;
    aHasProcessedStartupModules = false;

    {  // Scope for lock
      // Lock mQueuedEvents to steal its contents, and
      // mHasProcessedStartupModules to see if we can skip some steps.
      // Only trivial (loader lock friendly) code allowed here!
      MutexAutoLock lock(mMutex);
      aHasProcessedStartupModules = mHasProcessedStartupModules;
      mQueuedEvents.swap(queuedEvents);
    }

    if (!mEvaluator) {
      return;
    }

    Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy> processedEvents;
    int errorModules = 0;

    HashSet<uintptr_t, DefaultHasher<uintptr_t>, InfallibleAllocPolicy>
        newTrustedModuleBases;

    // Process queued events, weeding out trusted items as we go.
    for (auto& e : queuedEvents) {
      // Create a copy of the event without its modules; we'll then fill them
      // in, filtering out any trusted modules we can ignore.
      ModuleLoadEvent eventCopy(
          e, ModuleLoadEvent::CopyOption::CopyWithoutModules);
      for (auto& m : e.mModules) {
        bool ok = m.PrepForTelemetry();
        MOZ_ASSERT(ok);
        if (!ok) {
          continue;
        }

        Maybe<bool> maybeIsTrusted =
            mEvaluator.IsModuleTrusted(m, eventCopy, dllSvcRef.get());

        // Save xul.dll load timing for the ping payload.
        if ((m.mTrustFlags & ModuleTrustFlags::Xul) &&
            mXULLoadDurationMS.isNothing()) {
          mXULLoadDurationMS = m.mLoadDurationMS;
        }

        if (maybeIsTrusted.isNothing()) {
          // If there was an error, assume the DLL is trusted to avoid
          // flooding the telemetry packet, but record that an error occurred.
          errorModules++;
        } else if (maybeIsTrusted.value()) {
          // Module is trusted. If we haven't yet processed startup modules,
          // we need to remember it.
          if (!aHasProcessedStartupModules) {
            Unused << newTrustedModuleBases.put(m.mBase);
          }
        } else {
          // Module is untrusted; record it.
          Unused << eventCopy.mModules.append(std::move(m));
        }
      }

      if (eventCopy.mModules.empty()) {
        continue;
      }

      Unused << processedEvents.emplaceBack(std::move(eventCopy));
    }

    // Process the stacks. processedStacks will be element-for-element
    // in sync with processedEvents
    Vector<Telemetry::ProcessedStack, 0, InfallibleAllocPolicy> processedStacks;
    for (auto&& eventCopy : processedEvents) {
      std::vector<uintptr_t> stdCopy;
      for (auto&& f : eventCopy.mStack) {
        stdCopy.emplace_back(std::move(f));
      }
      Unused << processedStacks.emplaceBack(
          stackProcessor.GetStackAndModules(stdCopy));
    }

    {  // Scope for lock
      // Lock mTrustedModuleHistory and mProcessedEvents in order to merge the
      // data we just processed.
      // Only trivial (loader lock friendly) code allowed here!
      MutexAutoLock lock(mMutex);
      for (auto it = newTrustedModuleBases.iter(); !it.done(); it.next()) {
        Unused << mTrustedModuleHistory.put(it.get());
      }

      mErrorModules += errorModules;

      for (size_t i = 0; i < processedEvents.length(); ++i) {
        auto&& processedEvent = processedEvents[i];
        size_t newIndex = mProcessedStacks.AddStack(processedStacks[i]);
        // CombinedStacks is circular, so as its buffer rolls over, follow it
        // to keep indices in sync.
        if ((newIndex + 1) > mProcessedEvents.length()) {
          Unused << mProcessedEvents.append(std::move(processedEvent));
        } else {
          mProcessedEvents[newIndex] = std::move(processedEvent);
        }
      }
    }
  }

  /**
   * Looks at the currently-loaded module list, subtracts modules we've seen
   * before, and adds the remainder to the list of queued events. The idea is
   * to process modules that loaded before we started examining load events.
   *
   * @param  aHasProcessedStartupModules [in] The value of
   *                                     mHasProcessedStartupModules as received
   *                                     by a previous call. This is to avoid
   *                                     unnecessary locking.
   * @return true if any events were added to mQueuedEvents.
   */
  bool ProcessStartupModules(bool aHasProcessedStartupModules) {
    MOZ_ASSERT(!NS_IsMainThread());

    // Hold a reference to DllServices to ensure the object doesn't get deleted
    // during this call.
    RefPtr<DllServices> dllSvcRef(DllServices::Get());
    if (!dllSvcRef) {
      return false;
    }
    // Prevent static analysis build warnings about unused "kungFuDeathGrip"
    Unused << dllSvcRef;

    if (aHasProcessedStartupModules) {
      return false;
    }

    Vector<uintptr_t, 0, InfallibleAllocPolicy> allModuleBases =
        GetProcessModuleBases();

    Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy> startupEvents;

    {  // Scope for lock
      // Lock mTrustedModuleHistory and mProcessedEvents in order to form
      // list of startup modules.
      // Only trivial (loader lock friendly) code allowed here!
      MutexAutoLock lock(mMutex);

      mHasProcessedStartupModules = true;

      for (auto& base : allModuleBases) {
        // Look for it in mTrustedModuleHistory
        if (mTrustedModuleHistory.has(base)) {
          continue;
        }

        // Look for it in mProcessedEvents
        bool wasFound = false;
        for (auto& e : mProcessedEvents) {
          for (auto& m : e.mModules) {
            if (m.mBase == base) {
              wasFound = true;
            }
          }
        }
        if (wasFound) {
          continue;
        }

        // It's never been seen before so it must be a startup module.
        // One module = one event here.
        ModuleLoadEvent::ModuleInfo mi(base);
        ModuleLoadEvent e;
        e.mIsStartup = true;
        e.mProcessUptimeMS = 0;
        Unused << e.mModules.emplaceBack(std::move(mi));
        Unused << startupEvents.emplaceBack(std::move(e));
      }

      // Since we process startup modules only once, this data is no longer
      // needed.
      mTrustedModuleHistory.clearAndCompact();
    }

    if (startupEvents.empty()) {
      return false;
    }

    // Fill out more info in startupEvents.
    for (auto& e : startupEvents) {
      MOZ_ASSERT(e.mModules.length() == 1);
      ModuleLoadEvent::ModuleInfo& mi(e.mModules[0]);
      mi.PopulatePathInfo();
    }

    // Lock mQueuedEvents to add the new items.
    // Only trivial (loader lock friendly) code allowed here!
    MutexAutoLock lock(mMutex);
    for (auto&& e : startupEvents) {
      Unused << mQueuedEvents.emplaceBack(std::move(e));
    }
    return true;
  }

  bool GetTelemetryData(UntrustedModuleLoadTelemetryData& aOut) {
    MOZ_ASSERT(!NS_IsMainThread());

    // Hold a reference to DllServices to ensure the object doesn't get deleted
    // during this call.
    RefPtr<DllServices> dllSvcRef(DllServices::Get());
    if (!dllSvcRef) {
      return false;
    }
    // Prevent static analysis build warnings about unused "kungFuDeathGrip"
    Unused << dllSvcRef;

    bool hasProcessedStartupModules = false;
    ProcessQueuedEvents(hasProcessedStartupModules);
    if (ProcessStartupModules(hasProcessedStartupModules)) {
      // New events were added; process those too.
      ProcessQueuedEvents(hasProcessedStartupModules);
    }

    aOut.mErrorModules = mErrorModules;
    aOut.mXULLoadDurationMS = mXULLoadDurationMS;

    // Lock mProcessedEvents and mProcessedStacks to make a copy for the caller.
    // Only trivial (loader lock friendly) code allowed here!
    MutexAutoLock lock(mMutex);

    aOut.mStacks = mProcessedStacks;
    for (auto& e : mProcessedEvents) {
      Unused << aOut.mEvents.append(e);
    }
    return true;
  }
};

const char* DllServices::kTopicDllLoadedMainThread = "dll-loaded-main-thread";
const char* DllServices::kTopicDllLoadedNonMainThread =
    "dll-loaded-non-main-thread";

DllServices* DllServices::Get() {
  static StaticLocalRefPtr<DllServices> sInstance(
      []() -> already_AddRefed<DllServices> {
        RefPtr<DllServices> dllSvc(new DllServices());
        dllSvc->EnableFull();

        auto setClearOnShutdown = [ptr = &sInstance]() -> void {
          ClearOnShutdown(ptr);
        };

        if (NS_IsMainThread()) {
          setClearOnShutdown();
          return dllSvc.forget();
        }

        SystemGroup::Dispatch(
            TaskCategory::Other,
            NS_NewRunnableFunction("mozilla::DllServices::Get",
                                   std::move(setClearOnShutdown)));

        return dllSvc.forget();
      }());

  return sInstance;
}

DllServices::DllServices()
    : mUntrustedModulesManager(new UntrustedModulesManager()) {}

bool DllServices::GetUntrustedModuleTelemetryData(
    UntrustedModuleLoadTelemetryData& aOut) {
  return mUntrustedModulesManager->GetTelemetryData(aOut);
}

void DllServices::NotifyDllLoad(const bool aIsMainThread,
                                const nsString& aDllName) {
  const char* topic;

  if (aIsMainThread) {
    topic = kTopicDllLoadedMainThread;
  } else {
    topic = kTopicDllLoadedNonMainThread;
  }

  nsCOMPtr<nsIObserverService> obsServ(mozilla::services::GetObserverService());
  obsServ->NotifyObservers(nullptr, topic, aDllName.get());
}

void DllServices::NotifyUntrustedModuleLoads(
    const Vector<glue::ModuleLoadEvent, 0, InfallibleAllocPolicy>& aEvents) {
  mUntrustedModulesManager->OnNewEvents(aEvents);
}

}  // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPerformanceStats_h
#define nsPerformanceStats_h

#include "jsapi.h"

#include "nsHashKeys.h"
#include "nsTHashtable.h"

#include "nsIObserver.h"
#include "nsPIDOMWindow.h"

#include "nsIPerformanceStats.h"

class nsPerformanceGroup;

typedef mozilla::Vector<RefPtr<js::PerformanceGroup>> JSGroupVector;
typedef mozilla::Vector<RefPtr<nsPerformanceGroup>> GroupVector;

/**
 * An implementation of the nsIPerformanceStatsService.
 *
 * Note that this implementation is not thread-safe.
 */
class nsPerformanceStatsService : public nsIPerformanceStatsService,
                                  public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCESTATSSERVICE
  NS_DECL_NSIOBSERVER

  nsPerformanceStatsService();
  nsresult Init();

private:
  nsresult InitInternal();
  void Dispose();
  virtual ~nsPerformanceStatsService();

protected:
  friend nsPerformanceGroup;

  /**
   * A unique identifier for the process.
   *
   * Process HANDLE under Windows, pid under Unix.
   */
  const uint64_t mProcessId;

  /**
   * The JS Runtime for the main thread.
   */
  JSRuntime* const mRuntime;

  /**
   * Generate unique identifiers.
   */
  uint64_t GetNextId();
  uint64_t mUIdCounter;



  /**
   * Extract a snapshot of performance statistics from a performance group.
   */
  static nsIPerformanceStats* GetStatsForGroup(const js::PerformanceGroup* group);
  static nsIPerformanceStats* GetStatsForGroup(const nsPerformanceGroup* group);



  /**
   * Get the performance groups associated to a given JS compartment.
   *
   * A compartment is typically associated to the following groups:
   * - the top group, shared by the entire process;
   * - the window group, if the code is executed in a window, shared
   *     by all compartments for that window (typically, all frames);
   * - the add-on group, if the code is executed as an add-on, shared
   *     by all compartments for that add-on (typically, all modules);
   * - the compartment's own group.
   *
   * Pre-condition: the VM must have entered the JS compartment.
   *
   * The caller is expected to cache the results of this method, as
   * calling it more than once may not return the same instances of
   * performance groups.
   */
  bool GetPerformanceGroups(JSContext* cx, JSGroupVector&);
  static bool GetPerformanceGroupsCallback(JSContext* cx, JSGroupVector&, void* closure);



  /**********************************************************
   *
   * Sets of all performance groups, indexed by several keys.
   *
   * These sets do not keep the performance groups alive. Rather, a
   * performance group is inserted in the relevant sets upon
   * construction and removed from the sets upon destruction or when
   * we Dispose() of the service.
   *
   * A `nsPerformanceGroup` is typically kept alive (as a
   * `js::PerformanceGroup`) by the JSCompartment to which it is
   * associated. It may also temporarily be kept alive by the JS
   * stack, in particular in case of nested event loops.
   */

  /**
   * Set of performance groups associated to add-ons, indexed
   * by add-on id. Each item is shared by all the compartments
   * that belong to the add-on.
   */
  struct AddonIdToGroup: public nsStringHashKey {
    explicit AddonIdToGroup(const nsAString* key)
      : nsStringHashKey(key)
      , mGroup(nullptr)
    { }
    nsPerformanceGroup* mGroup;
  };
  nsTHashtable<AddonIdToGroup> mAddonIdToGroup;

  /**
   * Set of performance groups associated to windows, indexed by outer
   * window id. Each item is shared by all the compartments that
   * belong to the window.
   */
  struct WindowIdToGroup: public nsUint64HashKey {
    explicit WindowIdToGroup(const uint64_t* key)
      : nsUint64HashKey(key)
      , mGroup(nullptr)
    {}
    nsPerformanceGroup* mGroup;
  };
  nsTHashtable<WindowIdToGroup> mWindowIdToGroup;

  /**
   * Set of all performance groups.
   */
  struct Groups: public nsPtrHashKey<nsPerformanceGroup> {
    explicit Groups(const nsPerformanceGroup* key)
      : nsPtrHashKey<nsPerformanceGroup>(key)
    {}
  };
  nsTHashtable<Groups> mGroups;

  /**
   * The performance group representing the runtime itself.  All
   * compartments are associated to this group.
   */
  RefPtr<nsPerformanceGroup> mTopGroup;

  /**********************************************************
   *
   * Measuring and recording the CPU use of the system.
   *
   */

  /**
   * Get the OS-reported time spent in userland/systemland, in
   * microseconds. On most platforms, this data is per-thread,
   * but on some platforms we need to fall back to per-process.
   *
   * Data is not guaranteed to be monotonic.
   */
  nsresult GetResources(uint64_t* userTime, uint64_t* systemTime) const;

  /**
   * Amount of user/system CPU time used by the thread (or process,
   * for platforms that don't support per-thread measure) since start.
   * Updated by `StopwatchStart` at most once per event.
   *
   * Unit: microseconds.
   */
  uint64_t mUserTimeStart;
  uint64_t mSystemTimeStart;


  /**********************************************************
   *
   * Callbacks triggered by the JS VM when execution of JavaScript
   * code starts/completes.
   *
   * As measures of user CPU time/system CPU time have low resolution
   * (and are somewhat slow), we measure both only during the calls to
   * `StopwatchStart`/`StopwatchCommit` and we make the assumption
   * that each group's user/system CPU time is proportional to the
   * number of clock cycles spent executing code in the group between
   * `StopwatchStart`/`StopwatchCommit`.
   *
   * The results may be skewed by the thread being rescheduled to a
   * different CPU during the measure, but we expect that on average,
   * the skew will have limited effects, and will generally tend to
   * make already-slow executions appear slower.
   */

  /**
   * Execution of JavaScript code has started. This may happen several
   * times in succession if the JavaScript code contains nested event
   * loops, in which case only the innermost call will receive
   * `StopwatchCommitCallback`.
   *
   * @param iteration The number of times we have started executing
   * JavaScript code.
   */
  static bool StopwatchStartCallback(uint64_t iteration, void* closure);
  bool StopwatchStart(uint64_t iteration);

  /**
   * Execution of JavaScript code has reached completion (including
   * enqueued microtasks). In cse of tested event loops, any ongoing
   * measurement on outer loops is silently cancelled without any call
   * to this method.
   *
   * @param iteration The number of times we have started executing
   * JavaScript code.
   * @param recentGroups The groups that have seen activity during this
   * event.
   */
  static bool StopwatchCommitCallback(uint64_t iteration, JSGroupVector& recentGroups, void* closure);
  bool StopwatchCommit(uint64_t iteration, JSGroupVector& recentGroups);

  /**
   * The number of times we have started executing JavaScript code.
   */
  uint64_t mIteration;

  /**
   * Commit performance measures of a single group.
   *
   * Data is transfered from `group->recent*` to `group->data`.
   *
   *
   * @param iteration The current iteration.
   * @param userTime The total user CPU time for this thread (or
   *   process, if per-thread data is not available) between the
   *   calls to `StopwatchStart` and `StopwatchCommit`.
   * @param systemTime The total system CPU time for this thread (or
   *   process, if per-thread data is not available) between the
   *   calls to `StopwatchStart` and `StopwatchCommit`.
   * @param cycles The total number of cycles for this thread
   *   between the calls to `StopwatchStart` and `StopwatchCommit`.
   * @param group The group containing the data to commit.
   */
  void CommitGroup(uint64_t iteration,
                   uint64_t userTime, uint64_t systemTime,  uint64_t cycles,
                   nsPerformanceGroup* group);




  /**********************************************************
   *
   * To check whether our algorithm makes sense, we keep count of the
   * number of times the process has been rescheduled to another CPU
   * while we were monitoring the performance of a group and we upload
   * this data through Telemetry.
   */
  nsresult UpdateTelemetry();

  uint64_t mProcessStayed;
  uint64_t mProcessMoved;
  uint32_t mProcessUpdateCounter;

  /**********************************************************
   *
   * Options controlling measurements.
   */

  /**
   * Determine if we are measuring the performance of every individual
   * compartment (in particular, every individual module, frame,
   * sandbox). Note that this makes measurements noticeably slower.
   */
  bool mIsMonitoringPerCompartment;
};



/**
 * Container for performance data.
 *
 * All values are monotonic.
 *
 * All values are updated after running to completion.
 */
struct PerformanceData {
  /**
   * Number of times we have spent at least 2^n consecutive
   * milliseconds executing code in this group.
   * durations[0] is increased whenever we spend at least 1 ms
   * executing code in this group
   * durations[1] whenever we spend 2ms+
   * ...
   * durations[i] whenever we spend 2^ims+
   */
  uint64_t mDurations[10];

  /**
   * Total amount of time spent executing code in this group, in
   * microseconds.
   */
  uint64_t mTotalUserTime;
  uint64_t mTotalSystemTime;
  uint64_t mTotalCPOWTime;

  /**
   * Total number of times code execution entered this group, since
   * process launch. This may be greater than the number of times we
   * have entered the event loop.
   */
  uint64_t mTicks;

  PerformanceData();
  PerformanceData(const PerformanceData& from) = default;
  PerformanceData& operator=(const PerformanceData& from) = default;
};



/**
 * Identification information for an item that can hold performance
 * data.
 */
class nsPerformanceGroupDetails {
public:
  nsPerformanceGroupDetails(const nsAString& aName,
                            const nsAString& aGroupId,
                            const nsAString& aAddonId,
                            const uint64_t aWindowId,
                            const uint64_t aProcessId,
                            const bool aIsSystem)
    : mName(aName)
    , mGroupId(aGroupId)
    , mAddonId(aAddonId)
    , mWindowId(aWindowId)
    , mProcessId(aProcessId)
    , mIsSystem(aIsSystem)
  { }
public:
  const nsAString& Name() const;
  const nsAString& GroupId() const;
  const nsAString& AddonId() const;
  uint64_t WindowId() const;
  uint64_t ProcessId() const;
  bool IsAddon() const;
  bool IsWindow() const;
  bool IsSystem() const;
private:
  const nsString mName;
  const nsString mGroupId;
  const nsString mAddonId;
  const uint64_t mWindowId;
  const uint64_t mProcessId;
  const bool mIsSystem;
};

/**
 * The kind of compartments represented by this group.
 */
enum class PerformanceGroupScope {
  /**
   * This group represents the entire runtime (i.e. the thread).
   */
  RUNTIME,

  /**
   * This group represents all the compartments executed in a window.
   */
  WINDOW,

  /**
   * This group represents all the compartments provided by an addon.
   */
  ADDON,

  /**
   * This group represents a single compartment.
   */
  COMPARTMENT,
};

/**
 * A concrete implementation of `js::PerformanceGroup`, also holding
 * performance data. Instances may represent individual compartments,
 * windows, addons or the entire runtime.
 *
 * This class is intended to be the sole implementation of
 * `js::PerformanceGroup`.
 */
class nsPerformanceGroup final: public js::PerformanceGroup,
                                public nsPerformanceGroupDetails
{
public:

  // Ideally, we would define the enum class in nsPerformanceGroup,
  // but this seems to choke some versions of gcc.
  typedef PerformanceGroupScope GroupScope;

  /**
   * Construct a performance group.
   *
   * @param rt The container runtime. Used to generate a unique identifier.
   * @param service The performance service. Used during destruction to
   *   cleanup the hash tables.
   * @param name A name for the group, designed mostly for debugging purposes,
   *   so it should be at least somewhat human-readable.
   * @param addonId The identifier of the add-on. Should be "" when the
   *   group is not part of an add-on,
   * @param windowId The identifier of the window. Should be 0 when the
   *   group is not part of a window.
   * @param processId A unique identifier for the process.
   * @param isSystem `true` if the code of the group is executed with
   *   system credentials, `false` otherwise.
   * @param scope the scope of this group.
   */
  static nsPerformanceGroup*
    Make(JSRuntime* rt,
         nsPerformanceStatsService* service,
         const nsAString& name,
         const nsAString& addonId,
         uint64_t windowId,
         uint64_t processId,
         bool isSystem,
         GroupScope scope);

  /**
   * Utility: type-safer conversion from js::PerformanceGroup to nsPerformanceGroup.
   */
  static inline nsPerformanceGroup* Get(js::PerformanceGroup* self) {
    return static_cast<nsPerformanceGroup*>(self);
  }
  static inline const nsPerformanceGroup* Get(const js::PerformanceGroup* self) {
    return static_cast<const nsPerformanceGroup*>(self);
  }

  /**
   * The performance data committed to this group.
   */
  PerformanceData data;

  /**
   * The scope of this group. Used to determine whether the group
   * should be (de)activated.
   */
  GroupScope Scope() const;

  /**
   * Cleanup any references.
   */
  void Dispose();
protected:
  nsPerformanceGroup(nsPerformanceStatsService* service,
                     const nsAString& name,
                     const nsAString& groupId,
                     const nsAString& addonId,
                     uint64_t windowId,
                     uint64_t processId,
                     bool isSystem,
                     GroupScope scope);


  /**
   * Virtual implementation of `delete`, to make sure that objects are
   * destoyed with an implementation of `delete` compatible with the
   * implementation of `new` used to allocate them.
   *
   * Called by SpiderMonkey.
   */
  virtual void Delete() override {
    delete this;
  }
  virtual ~nsPerformanceGroup();

private:
  /**
   * The stats service. Used to perform cleanup during destruction.
   */
  RefPtr<nsPerformanceStatsService> mService;

  /**
   * The scope of this group. Used to determine whether the group
   * should be (de)activated.
   */
  const GroupScope mScope;
};

#endif

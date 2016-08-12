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
class nsPerformanceGroupDetails;

typedef mozilla::Vector<RefPtr<nsPerformanceGroup>> GroupVector;

/**
 * A data structure for registering observers interested in
 * performance alerts.
 *
 * Each performance group owns a single instance of this class.
 * Additionally, the service owns instances designed to observe the
 * performance alerts in all add-ons (respectively webpages).
 */
class nsPerformanceObservationTarget final: public nsIPerformanceObservable {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCEOBSERVABLE

  /**
   * `true` if this target has at least once performance observer
   * registered, `false` otherwise.
   */
  bool HasObservers() const;

  /**
   * Notify all the observers that jank has happened.
   */
  void NotifyJankObservers(nsIPerformanceGroupDetails* source, nsIPerformanceAlert* gravity);

  /**
   * Set the details on the group being observed.
   */
  void SetTarget(nsPerformanceGroupDetails* details);

private:
  ~nsPerformanceObservationTarget() {}

  // The observers for this target. We hold them as a vector, despite
  // the linear removal cost, as we expect that the typical number of
  // observers will be lower than 3, and that (un)registrations will
  // be fairly infrequent.
  mozilla::Vector<nsCOMPtr<nsIPerformanceObserver>> mObservers;

  // Details on the group being observed. May be `nullptr`.
  RefPtr<nsPerformanceGroupDetails> mDetails;
};

/**
 * The base class for entries of maps from addon id/window id to
 * performance group.
 *
 * Performance observers may be registered before their group is
 * created (e.g., one may register an observer for an add-on before
 * all its modules are loaded, or even before the add-on is loaded at
 * all or for an observer for a webpage before all its iframes are
 * loaded). This class serves to hold the observation target until the
 * performance group may be created, and then to associate the
 * observation target and the performance group.
 */
class nsGroupHolder {
public:
  nsGroupHolder()
    : mGroup(nullptr)
    , mPendingObservationTarget(nullptr)
  { }

  /**
   * Get the observation target, creating it if necessary.
   */
  nsPerformanceObservationTarget* ObservationTarget();

  /**
   * Get the group, if it has been created.
   *
   * May return `null` if the group hasn't been created yet.
   */
  class nsPerformanceGroup* GetGroup();

  /**
   * Set the group.
   *
   * Once this method has been called, calling
   * `this->ObservationTarget()` and `group->ObservationTarget()` is equivalent.
   *
   * Must only be called once.
   */
  void SetGroup(class nsPerformanceGroup*);
private:
  // The group. Initially `nullptr`, until we have called `SetGroup`.
  class nsPerformanceGroup* mGroup;

  // The observation target. Instantiated by the first call to
  // `ObservationTarget()`.
  RefPtr<nsPerformanceObservationTarget> mPendingObservationTarget;
};

/**
 * An implementation of the nsIPerformanceStatsService.
 *
 * Note that this implementation is not thread-safe.
 */
class nsPerformanceStatsService final : public nsIPerformanceStatsService,
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
  ~nsPerformanceStatsService();

protected:
  friend nsPerformanceGroup;

  /**
   * `false` until `Init()` and after `Dispose()`, `true` inbetween.
   */
  bool mIsAvailable;

  /**
   * `true` once we have called `Dispose()`.
   */
  bool mDisposed;

  /**
   * A unique identifier for the process.
   *
   * Process HANDLE under Windows, pid under Unix.
   */
  const uint64_t mProcessId;

  /**
   * The JS Context for the main thread.
   */
  JSContext* const mContext;

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
  bool GetPerformanceGroups(JSContext* cx, js::PerformanceGroupVector&);
  static bool GetPerformanceGroupsCallback(JSContext* cx, js::PerformanceGroupVector&, void* closure);



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
  struct AddonIdToGroup: public nsStringHashKey,
                         public nsGroupHolder {
    explicit AddonIdToGroup(const nsAString* key)
      : nsStringHashKey(key)
    { }
  };
  nsTHashtable<AddonIdToGroup> mAddonIdToGroup;

  /**
   * Set of performance groups associated to windows, indexed by outer
   * window id. Each item is shared by all the compartments that
   * belong to the window.
   */
  struct WindowIdToGroup: public nsUint64HashKey,
                          public nsGroupHolder {
    explicit WindowIdToGroup(const uint64_t* key)
      : nsUint64HashKey(key)
    {}
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

  bool mIsHandlingUserInput;

  /**
   * The number of user inputs since the start of the process. Used to
   * determine whether the current iteration has triggered a
   * (JS-implemented) user input.
   */
  uint64_t mUserInputCount;

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
  static bool StopwatchCommitCallback(uint64_t iteration,
                                      js::PerformanceGroupVector& recentGroups,
                                      void* closure);
  bool StopwatchCommit(uint64_t iteration, js::PerformanceGroupVector& recentGroups);

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
   * @param isJankVisible If `true`, expect that the user will notice
   *   any slowdown.
   * @param group The group containing the data to commit.
   */
  void CommitGroup(uint64_t iteration,
                   uint64_t userTime, uint64_t systemTime,  uint64_t cycles,
                   bool isJankVisible,
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


  /**********************************************************
   *
   * Determining whether jank is user-visible.
   */

  /**
   * `true` if we believe that any slowdown can cause a noticeable
   * delay in handling user-input.
   *
   * In the current implementation, we return `true` if the latest
   * user input was less than MAX_DURATION_OF_INTERACTION_MS ago. This
   * includes all inputs (mouse, keyboard, other devices), with the
   * exception of mousemove.
   */
  bool IsHandlingUserInput();


public:
  /**********************************************************
   *
   * Letting observers register themselves to watch for performance
   * alerts.
   *
   * To avoid saturating clients with alerts (or even creating loops
   * of alerts), each alert is buffered. At the end of each iteration
   * of the event loop, groups that have caused performance alerts
   * are registered in a set of pending alerts, and the collection
   * timer hasn't been started yet, it is started. Once the timer
   * firers, we gather all the pending alerts, empty the set and
   * dispatch to observers.
   */

  /**
   * Clear the set of pending alerts and dispatch the pending alerts
   * to observers.
   */
  void NotifyJankObservers(const mozilla::Vector<uint64_t>& previousJankLevels);

private:
  /**
   * The set of groups for which we know that an alert should be
   * raised. This set is cleared once `mPendingAlertsCollector`
   * fires.
   *
   * Invariant: no group may appear twice in this vector.
   */
  GroupVector mPendingAlerts;

  /**
   * A timer callback in charge of collecting the groups in
   * `mPendingAlerts` and triggering `NotifyJankObservers` to dispatch
   * performance alerts.
   */
  RefPtr<class PendingAlertsCollector> mPendingAlertsCollector;


  /**
   * Observation targets that are not attached to a specific group.
   */
  struct UniversalTargets {
    UniversalTargets();
    /**
     * A target for observers interested in watching all addons.
     */
    RefPtr<nsPerformanceObservationTarget> mAddons;

    /**
     * A target for observers interested in watching all windows.
     */
    RefPtr<nsPerformanceObservationTarget> mWindows;
  };
  UniversalTargets mUniversalTargets;

  /**
   * The threshold, in microseconds, above which a performance group is
   * considered "slow" and should raise performance alerts.
   */
  uint64_t mJankAlertThreshold;

  /**
   * A buffering delay, in milliseconds, used by the service to
   * regroup performance alerts, before observers are actually
   * noticed. Higher delays let the system avoid redundant
   * notifications for the same group, and are generally better for
   * performance.
   */
  uint32_t mJankAlertBufferingDelay;

  /**
   * The threshold above which jank, as reported by the refresh drivers,
   * is considered user-visible.
   *
   * A value of n means that any jank above 2^n ms will be considered
   * user visible.
   */
  short mJankLevelVisibilityThreshold;

  /**
   * The number of microseconds during which we assume that a
   * user-interaction can keep the code jank-critical. Any user
   * interaction that lasts longer than this duration is expected to
   * either have already caused jank or have caused a nested event
   * loop.
   *
   * In either case, we consider that monitoring
   * jank-during-interaction after this duration is useless.
   */
  uint64_t mMaxExpectedDurationOfInteractionUS;
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
class nsPerformanceGroupDetails final: public nsIPerformanceGroupDetails {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCEGROUPDETAILS

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
  bool IsContentProcess() const;
private:
  ~nsPerformanceGroupDetails() {}

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
class nsPerformanceGroup final: public js::PerformanceGroup {
public:

  // Ideally, we would define the enum class in nsPerformanceGroup,
  // but this seems to choke some versions of gcc.
  typedef PerformanceGroupScope GroupScope;

  /**
   * Construct a performance group.
   *
   * @param cx The container context. Used to generate a unique identifier.
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
    Make(JSContext* cx,
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
   * Identification details for this group.
   */
  nsPerformanceGroupDetails* Details() const;

  /**
   * Cleanup any references.
   */
  void Dispose();

  /**
   * Set the observation target for this group.
   *
   * This method must be called exactly once, when the performance
   * group is attached to its `nsGroupHolder`.
   */
  void SetObservationTarget(nsPerformanceObservationTarget*);


  /**
   * `true` if we have already noticed that a performance alert should
   * be raised for this group but we have not dispatched it yet,
   * `false` otherwise.
   */
  bool HasPendingAlert() const;
  void SetHasPendingAlert(bool value);

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
  ~nsPerformanceGroup();

private:
  /**
   * Identification details for this group.
   */
  RefPtr<nsPerformanceGroupDetails> mDetails;

  /**
   * The stats service. Used to perform cleanup during destruction.
   */
  RefPtr<nsPerformanceStatsService> mService;

  /**
   * The scope of this group. Used to determine whether the group
   * should be (de)activated.
   */
  const GroupScope mScope;

// Observing performance alerts.

public:
  /**
   * The observation target, used to register observers.
   */
  nsPerformanceObservationTarget* ObservationTarget() const;

  /**
   * Record a jank duration.
   *
   * Update the highest recent jank if necessary.
   */
  void RecordJank(uint64_t jank);
  uint64_t HighestRecentJank();

  /**
   * Record a CPOW duration.
   *
   * Update the highest recent CPOW if necessary.
   */
  void RecordCPOW(uint64_t cpow);
  uint64_t HighestRecentCPOW();

  /**
   * Record that this group has recently been involved in handling
   * user input. Note that heuristics are involved here, so the
   * result is not 100% accurate.
   */
  void RecordUserInput();
  bool HasRecentUserInput();

  /**
   * Reset recent values (recent highest CPOW and jank, involvement in
   * user input).
   */
  void ResetRecent();
private:
  /**
   * The target used by observers to register for watching slow
   * performance alerts caused by this group.
   *
   * May be nullptr for groups that cannot be watched (the top group).
   */
  RefPtr<class nsPerformanceObservationTarget> mObservationTarget;

  /**
   * The highest jank encountered since jank observers for this group
   * were last called, in microseconds.
   */
  uint64_t mHighestJank;

  /**
   * The highest CPOW encountered since jank observers for this group
   * were last called, in microseconds.
   */
  uint64_t mHighestCPOW;

  /**
   * `true` if this group has been involved in handling user input,
   * `false` otherwise.
   *
   * Note that we use heuristics to determine whether a group is
   * involved in handling user input, so this value is not 100%
   * accurate.
   */
  bool mHasRecentUserInput;

  /**
   * `true` if this group has caused a performance alert and this alert
   * hasn't been dispatched yet.
   *
   * We use this as part of the buffering of performance alerts. If
   * the group generates several alerts several times during the
   * buffering delay, we only wish to add the group once to the list
   * of alerts.
   */
  bool mHasPendingAlert;
};

#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry.startuptelemetry

import android.app.Activity
import org.mozilla.focus.telemetry.startuptelemetry.AppStartReasonProvider.StartReason
import org.mozilla.focus.telemetry.startuptelemetry.StartupActivityLog.LogEntry

/**
 * Identifies the "state" of start up where state can be COLD/WARM/HOT and possibly others.
 *
 * This class is nuanced: **please read the kdoc carefully before using it.** Consider contacting
 * the perf team with your use case.
 *
 * For this class, we use the terminology from the [StartupActivityLog] such as STARTED and STOPPED.
 * However, we're assuming STARTED means foregrounded and STOPPED means backgrounded. If this
 * assumption is false, the logic in this class may be incorrect.
 */
class StartupStateProvider(
    private val startupLog: StartupActivityLog,
    private val startReasonProvider: AppStartReasonProvider
) {

    enum class StartupState {
        COLD, WARM, HOT,

        /**
         * A start up state where we weren't able to bucket it into the other categories.
         * This includes, but is not limited to:
         * - if the activity this is called from is not currently started
         * - if the currently started activity is not the first started activity
         */
        UNKNOWN;
    }

    /**
     * Returns the [StartupState] for the currently started activity. Note: the state will be
     * [StartupState.UNKNOWN] if the currently started activity is not the first started activity.
     *
     * This method must be called after the foreground Activity is STARTED.
     */
    fun getStartupStateForStartedActivity(activityClass: Class<out Activity>): StartupState = when {
        isColdStartForStartedActivity(activityClass) -> StartupState.COLD
        isWarmStartForStartedActivity(activityClass) -> StartupState.WARM
        isHotStartForStartedActivity(activityClass) -> StartupState.HOT
        else -> StartupState.UNKNOWN
    }

    /**
     * Returns true if the current startup state is COLD and the currently started activity is the
     * first started activity (i.e. we can use it for performance measurements).
     *
     * This method must be called after the foreground Activity is STARTED.
     */
    fun isColdStartForStartedActivity(activityClass: Class<out Activity>): Boolean {
        // A cold start means:
        // - the process was started for the first started activity (e.g. not a service)
        // - the first started activity ever is still active
        //
        // Thus, for the activity log we expect:
        //   [... Activity-STARTED, App-STARTED]
        // since if another Activity was started, it would appear after App-STARTED. This is where:
        // - the app has not been stopped ever
        if (startReasonProvider.reason != StartReason.ACTIVITY) {
            return false
        }

        val isLastStartedActivityStillStarted = startupLog.log.takeLast(2) == listOf(
            LogEntry.ActivityStarted(activityClass),
            LogEntry.AppStarted
        )
        return !startupLog.log.contains(LogEntry.AppStopped) && isLastStartedActivityStillStarted
    }

    /**
     * Returns true if the current startup state is WARM and the currently started activity is the
     * first started activity for this start up (i.e. we can use it for performance measurements).
     *
     * This method must be called after the foreground activity is STARTED.
     */
    fun isWarmStartForStartedActivity(activityClass: Class<out Activity>): Boolean {
        // A warm start means:
        // - the app was backgrounded and has since been started
        // - the first started activity since the app was started is still active.
        // - that activity was created before being started
        //
        // For the activity log, we expect:
        //   [... App-STOPPED, ... Activity-CREATED, Activity-STARTED, App-STARTED]
        // where:
        // - App-STOPPED is the last STOPPED seen
        // - we're assuming App-STARTED will only be last if one activity is started (as observed)
        if (!startupLog.log.contains(LogEntry.AppStopped)) {
            return false // if the app hasn't been stopped, it's not a warm start.
        }
        val afterLastStopped = startupLog.log.takeLastWhile { it != LogEntry.AppStopped }

        @Suppress("MagicNumber")
        return afterLastStopped.takeLast(3) == listOf(
            LogEntry.ActivityCreated(activityClass),
            LogEntry.ActivityStarted(activityClass),
            LogEntry.AppStarted
        )
    }

    /**
     * Returns true if the current startup state is HOT and the currently started activity is the
     * first started activity for this start up (i.e. we can use it for performance measurements).
     *
     * This method must be called after the foreground activity is STARTED.
     */
    fun isHotStartForStartedActivity(activityClass: Class<out Activity>): Boolean {
        // A hot start means:
        // - the app was backgrounded and has since been started
        // - the first started activity since the app was started is still active.
        // - that activity was not created before being started
        //
        // For the activity log, we expect:
        //   [... App-STOPPED, ... Activity-STARTED, App-STARTED]
        // where:
        // - App-STOPPED is the last STOPPED seen
        // - App-CREATED is NOT called for this activity
        // - we're assuming App-STARTED will only be last if one activity is started (as observed)
        if (!startupLog.log.contains(LogEntry.AppStopped)) {
            return false // if the app hasn't been stopped, it's not a hot start.
        }
        val afterLastStopped = startupLog.log.takeLastWhile { it != LogEntry.AppStopped }

        val isLastActivityStartedStillStarted = afterLastStopped.takeLast(2) == listOf(
            LogEntry.ActivityStarted(activityClass),
            LogEntry.AppStarted
        )
        return !afterLastStopped.contains(LogEntry.ActivityCreated(activityClass)) &&
            isLastActivityStartedStillStarted
    }
}

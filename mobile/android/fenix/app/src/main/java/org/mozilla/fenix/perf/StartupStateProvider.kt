/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.Activity
import org.mozilla.fenix.perf.AppStartReasonProvider.StartReason
import org.mozilla.fenix.perf.StartupActivityLog.LogEntry

/**
 * Identifies the "state" of start up where state can be COLD/WARM/HOT and possibly others. See
 * the [Fenix perf glossary](https://wiki.mozilla.org/index.php?title=Performance/Fenix/Glossary)
 * for specific definitions.
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
     * A short-circuit implementation of [isColdStartForStartedActivity] that will return false early
     * so we don't have to call [isColdStartForStartedActivity].
     *
     * When this can be called might be tightly coupled to [ColdStartupDurationTelemetry]: use at
     * your own risk.
     */
    fun shouldShortCircuitColdStart(): Boolean = startupLog.log.contains(LogEntry.AppStopped)
}

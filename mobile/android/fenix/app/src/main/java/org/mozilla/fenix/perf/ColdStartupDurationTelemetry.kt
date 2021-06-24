/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.content.Intent
import android.os.SystemClock
import android.view.View
import androidx.core.view.doOnPreDraw
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.GleanMetrics.PerfStartup
import org.mozilla.fenix.HomeActivity
import java.util.concurrent.TimeUnit

private val logger = Logger("ColdStartupDuration")

/**
 * A class to record COLD start up telemetry. This class is intended to improve upon our mistakes from the
 * AppStartupTelemetry class by being simple-to-implement and
 * simple-to-analyze (i.e. works in GLAM) rather than being a "perfect" and comprehensive measurement.
 *
 * This class relies on external state providers like [StartupStateProvider] that are tricky to
 * implement correctly so take the results with a grain of salt.
 */
class ColdStartupDurationTelemetry {

    fun onHomeActivityOnCreate(
        visualCompletenessQueue: VisualCompletenessQueue,
        startupStateProvider: StartupStateProvider,
        safeIntent: SafeIntent,
        rootContainer: View
    ) {
        // Optimization: it might be expensive to post runnables so we can short-circuit
        // with a subset of the later logic.
        if (startupStateProvider.shouldShortCircuitColdStart()) {
            logger.debug("Not measuring: is not cold start (short-circuit)")
            return
        }

        rootContainer.doOnPreDraw {
            // This block takes 0ms on a Moto G5: it doesn't seem long enough to optimize.
            val firstFrameNanos = SystemClock.elapsedRealtimeNanos()
            if (startupStateProvider.isColdStartForStartedActivity(HomeActivity::class.java)) {
                visualCompletenessQueue.queue.runIfReadyOrQueue {
                    recordColdStartupTelemetry(safeIntent, firstFrameNanos)
                }
            }
        }
    }

    private fun recordColdStartupTelemetry(safeIntent: SafeIntent, firstFrameNanos: Long) {
        // This code duplicates the logic for determining how we should handle this intent which
        // could result in inconsistent results: e.g. the browser might get a VIEW intent but it's
        // malformed so the app treats it as a MAIN intent but here we record VIEW. However, the
        // logic for determining the end state is distributed and buried & inspecting the end state
        // is fragile (e.g. if the browser was open, was it a MAIN w/ session restore or VIEW?) so we
        // use this simpler solution even if it's imperfect. Hopefully, the success cases will
        // outnumber the edge cases into statistical insignificance.
        val (metric, typeForLog) = when (safeIntent.action) {
            Intent.ACTION_MAIN -> Pair(PerfStartup.coldMainAppToFirstFrame, "MAIN")
            Intent.ACTION_VIEW -> Pair(PerfStartup.coldViewAppToFirstFrame, "VIEW")
            else -> Pair(PerfStartup.coldUnknwnAppToFirstFrame, "UNKNOWN")
        }

        val startNanos = StartupTimeline.frameworkStartMeasurement.applicationInitNanos
        val durationMillis = TimeUnit.NANOSECONDS.toMillis(firstFrameNanos - startNanos)
        metric.accumulateSamples(longArrayOf(durationMillis))
        logger.info("COLD $typeForLog Application.<init> to first frame: $durationMillis ms")
    }
}

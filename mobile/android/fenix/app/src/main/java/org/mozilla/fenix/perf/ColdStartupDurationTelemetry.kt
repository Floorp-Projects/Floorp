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
import org.mozilla.fenix.perf.StartupActivityStateProvider.FirstForegroundActivity
import org.mozilla.fenix.perf.StartupActivityStateProvider.FirstForegroundActivityState
import org.mozilla.fenix.perf.AppStartReasonProvider.StartReason
import java.util.concurrent.TimeUnit

private val logger = Logger("ColdStartupDuration")

/**
 * A class to record COLD start up telemetry. This class is intended to improve upon our mistakes from the
 * [org.mozilla.fenix.components.metrics.AppStartupTelemetry] class by being simple-to-implement and
 * simple-to-analyze (i.e. works in GLAM) rather than being a "perfect" and comprehensive measurement.
 *
 * This class relies on external state providers like [AppStartReasonProvider] and
 * [StartupActivityStateProvider] that are tricky to implement correctly so take the results with a
 * grain of salt.
 */
class ColdStartupDurationTelemetry {

    fun onHomeActivityOnCreate(
        visualCompletenessQueue: VisualCompletenessQueue,
        startReasonProvider: AppStartReasonProvider,
        startupActivityStateProvider: StartupActivityStateProvider,
        safeIntent: SafeIntent,
        rootContainer: View
    ) {
        // Optimization: it's expensive to post runnables so we can short-circuit with a subset of the later logic.
        if (startupActivityStateProvider.firstForegroundActivityState ==
                FirstForegroundActivityState.AFTER_FOREGROUND) {
            logger.debug("Not measuring: first foreground activity already backgrounded")
            return
        }

        rootContainer.doOnPreDraw {
            // Optimization: we're running code before the first frame so we want to avoid doing anything
            // expensive as part of the drawing loop. Recording telemetry took 3-7ms on the Moto G5 (a
            // frame is ~16ms) so we defer the expensive work for later by posting a Runnable.
            //
            // We copy the values because their values may change when passed into the handler. It's
            // cheaper to copy the values than copy the objects (= allocation + copy values) so we just
            // copy the values even though this copy could happen incorrectly if these values become objects later.
            val startReason = startReasonProvider.reason
            val firstActivity = startupActivityStateProvider.firstForegroundActivityOfProcess
            val firstActivityState = startupActivityStateProvider.firstForegroundActivityState
            val firstFrameNanos = SystemClock.elapsedRealtimeNanos()

            // On the visual completeness queue, this will report later than posting to the main thread (not
            // ideal for pulling out of automated performance tests) but should delay visual completeness less.
            visualCompletenessQueue.queue.runIfReadyOrQueue {
                if (!isColdStartToThisHomeActivityInstance(startReason, firstActivity, firstActivityState)) {
                    logger.debug("Not measuring: this activity isn't both the first foregrounded & HomeActivity")
                    return@runIfReadyOrQueue
                }

                recordColdStartupTelemetry(safeIntent, firstFrameNanos)
            }
        }
    }

    private fun isColdStartToThisHomeActivityInstance(
        startReason: StartReason,
        firstForegroundActivity: FirstForegroundActivity,
        firstForegroundActivityState: FirstForegroundActivityState
    ): Boolean {
        // This logic is fragile: if an Activity that isn't currently foregrounded is refactored to get
        // temporarily foregrounded (e.g. IntentReceiverActivity) or an interstitial Activity is added
        // that is temporarily foregrounded, we'll no longer detect HomeActivity as the first foregrounded
        // activity and we'll never record telemetry.
        //
        // Because of this, we may not record values in Beta and Release if MigrationDecisionActivity
        // gets foregrounded (I never tested these channels: I think Nightly data is probably good enough for now).
        //
        // What we'd ideally determine is, "Is the final activity during this start up HomeActivity?"
        // However, it's challenging to do so in a robust way so we stick with this simpler solution
        // ("Is the first foregrounded activity during this start up HomeActivity?") despite its flaws.
        val wasProcessStartedBecauseOfAnActivity = startReason == StartReason.ACTIVITY
        val isThisTheFirstForegroundActivity = firstForegroundActivity == FirstForegroundActivity.HOME_ACTIVITY &&
            firstForegroundActivityState == FirstForegroundActivityState.CURRENTLY_FOREGROUNDED
        return wasProcessStartedBecauseOfAnActivity && isThisTheFirstForegroundActivity
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

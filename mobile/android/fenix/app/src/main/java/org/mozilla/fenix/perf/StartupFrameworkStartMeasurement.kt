/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.os.Process
import android.os.SystemClock
import java.io.FileNotFoundException
import kotlin.math.roundToLong
import org.mozilla.fenix.GleanMetrics.StartupTimeline as Telemetry

/**
 * A class to measure the time the Android framework executes before letting us execute our own code
 * for the first time in Application's initializer: this value is captured in the
 * [Telemetry.frameworkStart] probe.
 *
 * Since we cannot execute code at process start, this measurement does not fit within the Glean
 * Timespan metric start/stop programming model so it lives in its own class.
 */
internal class StartupFrameworkStartMeasurement(
    private val stat: Stat = Stat(),
    private val telemetry: Telemetry = Telemetry,
    private val getElapsedRealtimeNanos: () -> Long = SystemClock::elapsedRealtimeNanos
) {

    private var isMetricSet = false

    private var applicationInitNanos = -1L
    private var isApplicationInitCalled = false

    fun onApplicationInit() {
        // This gets called from multiple processes: don't do anything expensive. See call site for details.
        //
        // In the main process, there are multiple Application impl so we ensure it's only set by
        // the first one.
        if (!isApplicationInitCalled) {
            isApplicationInitCalled = true
            applicationInitNanos = getElapsedRealtimeNanos()
        }
    }

    /**
     * Sets the values for metrics to record in glean.
     *
     * We defer these metrics, rather than setting them as soon as the values are available,
     * because they are slow to fetch and we don't want to impact startup.
     */
    fun setExpensiveMetric() {
        // The application is only init once per process lifetime so we only set this value once.
        if (isMetricSet) return
        isMetricSet = true

        if (applicationInitNanos < 0) {
            telemetry.frameworkStartError.set(true)
        } else {
            val clockTicksPerSecond = stat.clockTicksPerSecond.also {
                // framework* is derived from the number of clock ticks per second. To ensure this
                // value does not throw off our result, we capture it too.
                telemetry.clockTicksPerSecondV2.set(it)
            }

            // In our brief analysis, clock ticks per second was overwhelmingly equal to 100. To make
            // analysis easier in GLAM, we split the results into two separate metrics. See the
            // metric descriptions for more details.
            @Suppress("MagicNumber") // it's more confusing to separate the comment above from the value declaration.
            val durationMetric =
                if (clockTicksPerSecond == 100L) telemetry.frameworkPrimary else telemetry.frameworkSecondary

            try {
                durationMetric.setRawNanos(getFrameworkStartNanos())
            } catch (e: FileNotFoundException) {
                // Privacy managers can add hooks that block access to reading system /proc files.
                // We want to catch these exception and report an error on accessing the file
                // rather than an implementation error.
                telemetry.frameworkStartReadError.set(true)
            }
        }
    }

    /**
     * @throws [java.io.FileNotFoundException]
     */
    private fun getFrameworkStartNanos(): Long {
        // Get our timestamps in ticks: we expect ticks to be less granular than nanoseconds so,
        // to ensure our measurement uses the correct number of significant figures, we convert
        // everything to ticks before getting the result.
        //
        // Similarly, we round app init to a whole integer tick value because process start only
        // comes in integer ticks values.
        val processStartTicks = stat.getProcessStartTimeTicks(Process.myPid())
        val applicationInitTicks = applicationInitNanos.nanosToTicks().roundToLong()

        val frameworkStartTicks = applicationInitTicks - processStartTicks

        // Glean only takes whole unit nanoseconds so we round to that. I'm not sure but it may be
        // possible that capturing nanos in a double will produce a rounding error that chops off
        // significant values. However, since we expect to be using a much smaller portion of the
        // nano field - if ticks are actually less granular than nanoseconds - I don't expect for
        // this to be a problem.
        return frameworkStartTicks.ticksToNanos().roundToLong()
    }

    private fun Long.nanosToTicks(): Double = stat.convertNanosToTicks(this)
    private fun Long.ticksToNanos(): Double = stat.convertTicksToNanos(this)
}

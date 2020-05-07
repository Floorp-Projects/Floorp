/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.os.SystemClock
import android.system.Os
import android.system.OsConstants
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import java.io.File
import java.util.concurrent.TimeUnit

private const val FIELD_POS_STARTTIME = 21 // starttime naming matches field in man page.

/**
 * Functionality from stat on the proc pseudo-filesystem common to unix systems. /proc contains
 * information related to active processes. /proc/$pid/stat contains information about the status of
 * the process by the given process id (pid).
 *
 * See the man page - `man 5 proc` - on linux for more information:
 *   http://man7.org/linux/man-pages/man5/proc.5.html
 */
open class Stat {

    /**
     * @throws [java.io.FileNotFoundException]
     */
    @VisibleForTesting(otherwise = PRIVATE)
    open fun getStatText(pid: Int): String = File("/proc/$pid/stat").readText()

    // See `man 3 sysconf` for details on Os.sysconf and OsConstants:
    //   http://man7.org/linux/man-pages/man3/sysconf.3.html
    open val clockTicksPerSecond: Long get() = Os.sysconf(OsConstants._SC_CLK_TCK)
    private val nanosPerClockTick = TimeUnit.SECONDS.toNanos(1).let { nanosPerSecond ->
        // We use nanos per clock tick, rather than clock ticks per nanos, to mitigate float/double
        // rounding errors: this way we can use integer values and divide the larger value by the smaller one.
        nanosPerSecond / clockTicksPerSecond.toDouble()
    }

    /**
     * Gets the process start time since system boot in ticks, including time spent in suspension/deep sleep.
     * This value can be compared against [SystemClock.elapsedRealtimeNanos]: you can convert between
     * measurements using [convertTicksToNanos] and [convertNanosToTicks].
     *
     * Ticks are "an arbitrary unit for measuring internal system time": https://superuser.com/a/101202
     * They are not aligned with CPU frequency and do not change at runtime but can theoretically
     * change between devices. On the Pixel 2, one tick is equivalent to one centisecond.
     *
     * We confirmed that this measurement and elapsedRealtimeNanos both include suspension time, and
     * are thus comparable, by* looking at their source:
     * - /proc/pid/stat starttime is set using boottime:
     * https://github.com/torvalds/linux/blob/79e178a57dae819ae724065b47c25720494cc9f2/fs/proc/array.c#L536
     * - elapsedRealtimeNanos is set using boottime:
     * https://cs.android.com/android/platform/superproject/+/master:system/core/libutils/SystemClock.cpp;l=60-68;drc=bab16584ce0525742b5370682c9132b2002ee110
     *
     * Perf note: this call reads from the pseudo-filesystem using the java File APIs, which isn't
     * likely to be a very optimized call path.
     *
     * Implementation inspired by https://stackoverflow.com/a/42195623.
     */
    fun getProcessStartTimeTicks(pid: Int): Long {
        return getStatText(pid).split(' ')[FIELD_POS_STARTTIME].toLong()
    }

    fun convertTicksToNanos(ticks: Long): Double = ticks * nanosPerClockTick
    fun convertNanosToTicks(nanos: Long): Double = nanos / nanosPerClockTick
}

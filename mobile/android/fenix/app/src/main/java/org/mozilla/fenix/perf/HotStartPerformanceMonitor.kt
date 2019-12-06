/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.os.SystemClock
import android.util.Log

/**
 * Monitors and reports elapsed time to complete hot startup of an Activity. Callers are expected
 * to call the appropriate lifecycle methods at the appropriate times.
 *
 * A "hot startup" is when the application moves from the background to the foreground and both the
 * Application/process and the Activity under measurement are already created.
 *
 * Unfortunately, this monitor does not capture the entire duration of hot start because the
 * framework calls several Android framework methods in the application's process before we are able
 * to add monitoring code (i.e. the first time our application code is called in onRestart). An
 * alternative implementation could measure performance from outside the application.
 *
 * The logs from this class are not visible to users by default. To see logs from this class, the
 * user must enable VERBOSE logging for the appropriate tag:
 *   adb shell setprop log.tag.FenixPerf VERBOSE
 */
class HotStartPerformanceMonitor(
    // We use VERBOSE logging so that the logs are not visible to users by default. We use the
    // Android Log methods to minimize overhead introduced in a-c logging.
    private val log: (String) -> Unit = { Log.v(Performance.TAG, it) },
    private val getElapsedRealtime: () -> Long = { SystemClock.elapsedRealtime() }
) {

    private var onRestartMillis: Long = -1

    fun onRestartFirstMethodCall() {
        onRestartMillis = getElapsedRealtime()
    }

    fun onPostResumeFinalMethodCall() {
        // If onRestart was never called, this is not a hot start: ignore it.
        if (onRestartMillis >= 0) {
            val elapsedMillis = getElapsedRealtime() - onRestartMillis
            log("hot start: $elapsedMillis")
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import mozilla.components.lib.crash.Crash

/**
 * Interface to be implemented by external services that collect telemetry about crash reports.
 */
interface CrashTelemetryService {
    /**
     * Records telemetry for this [Crash.UncaughtExceptionCrash].
     */
    fun record(crash: Crash.UncaughtExceptionCrash)

    /**
     * Records telemetry for this [Crash.NativeCodeCrash].
     */
    fun record(crash: Crash.NativeCodeCrash)

    /**
     * Records telemetry for this caught [Throwable] (non-crash).
     */
    fun record(throwable: Throwable)
}

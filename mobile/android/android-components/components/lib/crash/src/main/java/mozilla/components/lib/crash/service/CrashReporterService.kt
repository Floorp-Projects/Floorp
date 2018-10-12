/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import mozilla.components.lib.crash.Crash

/**
 * Interface to be implemented by external services that accept crash reports.
 */
interface CrashReporterService {
    /**
     * Submits a crash report for this [Crash.UncaughtExceptionCrash].
     */
    fun report(crash: Crash.UncaughtExceptionCrash)

    /**
     * Submits a crash report for this [Crash.NativeCodeCrash].
     */
    fun report(crash: Crash.NativeCodeCrash)
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import mozilla.components.lib.crash.Breadcrumb
import mozilla.components.lib.crash.Crash

internal const val INFO_PREFIX = "[INFO]"

/**
 * Interface to be implemented by external services that accept crash reports.
 */
interface CrashReporterService {
    /**
     * Submits a crash report for this [Crash.UncaughtExceptionCrash].
     *
     * @return Unique identifier that can be used by/with this crash reporter service to find this
     * crash - or null if no identifier can be provided.
     */
    fun report(crash: Crash.UncaughtExceptionCrash): String?

    /**
     * Submits a crash report for this [Crash.NativeCodeCrash].
     *
     * @return Unique identifier that can be used by/with this crash reporter service to find this
     * crash - or null if no identifier can be provided.
     */
    fun report(crash: Crash.NativeCodeCrash): String?

    /**
     * Submits a caught exception report for this [Throwable].
     *
     * @return Unique identifier that can be used by/with this crash reporter service to find this
     * crash - or null if no identifier can be provided.
     */
    fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String?
}

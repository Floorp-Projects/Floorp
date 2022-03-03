/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.lib.crash.Crash

const val LIB_CRASH_INFO_PREFIX = "[INFO]"

/**
 * Interface to be implemented by external services that accept crash reports.
 */
interface CrashReporterService {
    /**
     * A unique ID to identify this crash reporter service.
     */
    val id: String

    /**
     * A human-readable name for this crash reporter service (to be displayed in UI).
     */
    val name: String

    /**
     * Returns a URL to a website with the crash report if possible. Otherwise returns null.
     */
    fun createCrashReportUrl(identifier: String): String?

    /**
     * Submits a crash report for this [Crash.UncaughtExceptionCrash].
     *
     * @return Unique crash report identifier that can be used by/with this crash reporter service
     * to find this reported crash - or null if no identifier can be provided.
     */
    fun report(crash: Crash.UncaughtExceptionCrash): String?

    /**
     * Submits a crash report for this [Crash.NativeCodeCrash].
     *
     * @return Unique crash report identifier that can be used by/with this crash reporter service
     * to find this reported crash - or null if no identifier can be provided.
     */
    fun report(crash: Crash.NativeCodeCrash): String?

    /**
     * Submits a caught exception report for this [Throwable].
     *
     * @return Unique crash report identifier that can be used by/with this crash reporter service
     * to find this reported crash - or null if no identifier can be provided.
     */
    fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String?
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.rusterrors

import mozilla.appservices.errorsupport.ApplicationErrorReporter
import mozilla.appservices.errorsupport.setApplicationErrorReporter
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.base.crash.RustCrashReport

/**
 * Initialize application services error reporting
 *
 * Errors reports and breadcrumbs from Application Services will be forwarded
 * to the CrashReporting instance. Error counting, which is used for expected
 * errors like network errors, will be counted with Glean.
 */
public fun initializeRustErrors(crashReporter: CrashReporting) {
    setApplicationErrorReporter(AndroidComponentsErrorReportor(crashReporter))
}

internal class AppServicesErrorReport(
    override val typeName: String,
    override val message: String,
) : Exception(typeName), RustCrashReport

private class AndroidComponentsErrorReportor(
    val crashReporter: CrashReporting,
) : ApplicationErrorReporter {
    override fun reportError(typeName: String, message: String) {
        crashReporter.submitCaughtException(AppServicesErrorReport(typeName, message))
    }

    override fun reportBreadcrumb(message: String, module: String, line: UInt, column: UInt) {
        crashReporter.recordCrashBreadcrumb(Breadcrumb("$module[$line]: $message"))
    }
}

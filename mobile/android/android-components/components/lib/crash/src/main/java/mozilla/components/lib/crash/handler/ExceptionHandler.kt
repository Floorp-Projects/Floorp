/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.handler

import android.content.Context
import android.os.Process
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter

/**
 * [Thread.UncaughtExceptionHandler] implementation that forwards crashes to the [CrashReporter] instance.
 */
class ExceptionHandler(
    private val context: Context,
    private val crashReporter: CrashReporter,
    private val defaultExceptionHandler: Thread.UncaughtExceptionHandler? = null
) : Thread.UncaughtExceptionHandler {
    private var crashing = false

    override fun uncaughtException(thread: Thread, throwable: Throwable) {
        if (crashing) {
            return
        }

        try {
            crashing = true
            crashReporter.onCrash(context, Crash.UncaughtExceptionCrash(throwable,
                    crashReporter.crashBreadcrumbs.toSortedArrayList()))

            defaultExceptionHandler?.uncaughtException(thread, throwable)
        } finally {
            terminateProcess()
        }
    }

    private fun terminateProcess() {
        Process.killProcess(Process.myPid())
    }
}

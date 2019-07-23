/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.GleanMetrics.CrashMetrics
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.isMainProcess
import java.io.File

/**
 * A [CrashReporterService] implementation for recording metrics with Glean.  The purpose of this
 * crash reporter is to collect crash count metrics by capturing [Crash.UncaughtExceptionCrash] and
 * [Crash.NativeCodeCrash] events and record to the respective
 * [mozilla.components.service.glean.private.CounterMetricType].
 */
class GleanCrashReporterService(
    val context: Context,
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val file: File = File(context.applicationInfo.dataDir, CRASH_FILE_NAME)
) : CrashReporterService {
    companion object {
        // This file is stored in the application's data directory, so it should be located in the
        // same location as the application.
        // The format of this file is simple and uses the keys named below, one per line, to record
        // crashes.  That format allows for multiple crashes to be appended to the file if, for some
        // reason, the application cannot run and record them.
        const val CRASH_FILE_NAME = "glean_crash_counts"

        // These keys correspond to the labels found for crashCount metric in metrics.yaml as well
        // as the persisted crashes in the crash count file (see above comment)
        const val UNCAUGHT_EXCEPTION_KEY = "uncaught_exception"
        const val NATIVE_CODE_CRASH_KEY = "native_code_crash"
    }

    private val logger = Logger("glean/GleanCrashReporterService")

    init {
        run {
            // We only want to record things on the main process because that is the only one in which
            // Glean is properly initialized.  Checking to see if we are on the main process here will
            // prevent the situation that arises because the integrating app's Application will be
            // re-created when prompting to report the crash, and Glean is not initialized there since
            // it's not technically the main process.
            if (!context.isMainProcess()) {
                logger.info("GleanCrashReporterService initialized off of main process")
                return@run
            }

            if (!checkFileConditions()) {
                // checkFileConditions() internally logs error conditions
                return@run
            }

            // Parse the persisted crashes
            parseCrashFile()

            // Clear persisted counts by deleting the file
            file.delete()
        }
    }

    /**
     * Checks the file conditions to ensure it can be opened and read.
     *
     * @return True if the file exists and is able to be read, otherwise false
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun checkFileConditions(): Boolean {
        return if (!file.exists()) {
            // This is just an info line, as most of the time we hope there is no file which means
            // there were no crashes
            logger.info("No crashes to record, or file not found.")
            false
        } else if (!file.canRead()) {
            logger.error("Cannot read file")
            false
        } else if (!file.isFile) {
            logger.error("Expected file, but found directory")
            false
        } else {
            true
        }
    }

    /**
     * Parses the crashes collected in the persisted crash file.  The format of this file is simple,
     * each line may contain either [UNCAUGHT_EXCEPTION_KEY], or [NATIVE_CODE_CRASH_KEY] followed by
     * a newline character.
     *
     * Example:
     *
     * <--Beginning of file-->
     * uncaught_exception\n
     * uncaught_exception\n
     * native_code_crash\n
     * uncaught_exception\n
     * <--End of file-->
     *
     * It is unlikely that there will be more than one crash in a file, but not impossible.  This
     * could happen, for instance, if the application crashed again before the file could be
     * processed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun parseCrashFile() {
        val lines = file.readLines()
        var uncaughtExceptionCount = 0
        var nativeCodeCrashCount = 0

        // It's possible that there was more than one crash recorded in the file so process each
        // line and accumulate the crash counts.
        lines.forEach { line ->
            when (line) {
                UNCAUGHT_EXCEPTION_KEY -> ++uncaughtExceptionCount
                NATIVE_CODE_CRASH_KEY -> ++nativeCodeCrashCount
            }
        }

        // Now, record the crash counts into Glean
        CrashMetrics.crashCount[UNCAUGHT_EXCEPTION_KEY].add(uncaughtExceptionCount)
        CrashMetrics.crashCount[NATIVE_CODE_CRASH_KEY].add(nativeCodeCrashCount)
    }

    /**
     * This function handles the actual recording of the crash to the persisted crash file.  Since
     * we are only guaranteed runtime for the lifetime of the [report] function, anything that we do
     * in this function **MUST** be synchronous and blocking.  We cannot spawn work to background
     * processes or threads here if we want to guarantee that the work is completed. Also, since the
     * [report] functions are called synchronously, and from lib-crash's own process, it is unlikely
     * that this would be called from more than one place at the same time.
     *
     * @param crash Pass in the correct crash label to write to the file,
     *              either [UNCAUGHT_EXCEPTION_KEY] or [NATIVE_CODE_CRASH_KEY]
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun reportCrash(crash: String) {
        // Persist the crash in a file so that it can be recorded on the next application start. We
        // cannot directly record to Glean here because CrashHandler process is not the same process
        // as Glean is initialized in.
        // Create the file if it doesn't exist
        if (!file.exists()) {
            file.createNewFile()
        }

        // Add a line representing the crash that was received
        if (file.canWrite()) {
            file.appendText(crash + "\n")
        }
    }

    override fun report(crash: Crash.UncaughtExceptionCrash) {
        reportCrash(UNCAUGHT_EXCEPTION_KEY)
    }

    override fun report(crash: Crash.NativeCodeCrash) {
        reportCrash(NATIVE_CODE_CRASH_KEY)
    }
}

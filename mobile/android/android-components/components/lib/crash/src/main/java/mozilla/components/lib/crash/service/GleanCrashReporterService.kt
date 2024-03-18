/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.content.Context
import android.os.SystemClock
import androidx.annotation.VisibleForTesting
import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.SerializationException
import kotlinx.serialization.json.DecodeSequenceMode
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.decodeToSequence
import kotlinx.serialization.json.encodeToStream
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.GleanMetrics.CrashMetrics
import mozilla.components.lib.crash.GleanMetrics.Pings
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.isMainProcess
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.Date
import mozilla.components.lib.crash.GleanMetrics.Crash as GleanCrash

/**
 * A [CrashReporterService] implementation for recording metrics with Glean.  The purpose of this
 * crash reporter is to collect crash count metrics by capturing [Crash.UncaughtExceptionCrash],
 * [Throwable] and [Crash.NativeCodeCrash] events and record to the respective
 * [mozilla.components.service.glean.private.CounterMetricType].
 */
class GleanCrashReporterService(
    val context: Context,
    @get:VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val file: File = File(context.applicationInfo.dataDir, CRASH_FILE_NAME),
) : CrashTelemetryService {
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
        const val CAUGHT_EXCEPTION_KEY = "caught_exception"
        const val MAIN_PROCESS_NATIVE_CODE_CRASH_KEY = "main_proc_native_code_crash"
        const val FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY = "fg_proc_native_code_crash"
        const val BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY = "bg_proc_native_code_crash"

        // These keys are deprecated and should be removed after a period to allow for persisted
        // crashes to be submitted.
        const val FATAL_NATIVE_CODE_CRASH_KEY = "fatal_native_code_crash"
        const val NONFATAL_NATIVE_CODE_CRASH_KEY = "nonfatal_native_code_crash"
    }

    /**
     * The subclasses of GleanCrashAction are used to persist Glean actions to handle them later
     * (in the application which has Glean initialized). They are serialized to JSON objects and
     * appended to a file, in case multiple crashes occur prior to being able to submit the metrics
     * to Glean.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Serializable
    internal sealed class GleanCrashAction {
        /**
         * Submit the glean metrics/pings.
         */
        abstract fun submit()

        @Serializable
        @SerialName("count")
        data class Count(val label: String) : GleanCrashAction() {
            override fun submit() {
                CrashMetrics.crashCount[label].add()
            }
        }

        @Serializable
        @SerialName("ping")
        data class Ping(
            val uptimeNanos: Long,
            val processType: String,
            val timeMillis: Long,
            val startup: Boolean,
            val reason: Pings.crashReasonCodes,
            val cause: String = "os_fault",
            val remoteType: String = "",
        ) : GleanCrashAction() {
            override fun submit() {
                GleanCrash.uptime.setRawNanos(uptimeNanos)
                GleanCrash.processType.set(processType)
                GleanCrash.remoteType.set(remoteType)
                GleanCrash.time.set(Date(timeMillis))
                GleanCrash.startup.set(startup)
                GleanCrash.cause.set(cause)
                Pings.crash.submit(reason)
            }
        }
    }

    private val logger = Logger("glean/GleanCrashReporterService")
    private val creationTime = SystemClock.elapsedRealtimeNanos()

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
     * Calculates the application uptime based on the creation time of this class (assuming it is
     * created in the application's `OnCreate`).
     */
    private fun uptime() = SystemClock.elapsedRealtimeNanos() - creationTime

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
     * Parses the crashes collected in the persisted crash file. The format of this file is simple,
     * a stream of serialized JSON GleanCrashAction objects.
     *
     * Example:
     *
     * <--Beginning of file-->
     * {"type":"count","label":"uncaught_exception"}\n
     * {"type":"count","label":"uncaught_exception"}\n
     * {"type":"count","label":"main_process_native_code_crash"}\n
     * {"type":"ping","uptimeNanos":2000000,"processType":"main","timeMillis":42000000000,
     *  "startup":false}\n
     * <--End of file-->
     *
     * It is unlikely that there will be more than one crash in a file, but not impossible.  This
     * could happen, for instance, if the application crashed again before the file could be
     * processed.
     */
    @Suppress("ComplexMethod")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun parseCrashFile() {
        try {
            @OptIn(ExperimentalSerializationApi::class)
            val actionSequence = Json.decodeToSequence<GleanCrashAction>(
                file.inputStream(),
                DecodeSequenceMode.WHITESPACE_SEPARATED,
            )
            for (action in actionSequence) {
                action.submit()
            }
        } catch (e: IOException) {
            logger.error("Error reading crash file", e)
            return
        } catch (e: SerializationException) {
            logger.error("Error deserializing crash file", e)
            return
        }
    }

    /**
     * This function handles the actual recording of the crash to the persisted crash file. We are
     * only guaranteed runtime for the lifetime of the [CrashReporterService.report] function,
     * anything that we do in this function **MUST** be synchronous and blocking.  We cannot spawn
     * work to background processes or threads here if we want to guarantee that the work is
     * completed. Also, since the [CrashReporterService.report] functions are called synchronously,
     * and from lib-crash's own process, it is unlikely that this would be called from more than one
     * place at the same time.
     *
     * @param action Pass in the crash action to record.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun recordCrashAction(action: GleanCrashAction) {
        // Persist the crash in a file so that it can be recorded on the next application start. We
        // cannot directly record to Glean here because CrashHandler process is not the same process
        // as Glean is initialized in.
        // Create the file if it doesn't exist
        if (!file.exists()) {
            try {
                file.createNewFile()
            } catch (e: IOException) {
                logger.error("Failed to create crash file", e)
            }
        }

        // Add a line representing the crash that was received
        if (file.canWrite()) {
            try {
                @OptIn(ExperimentalSerializationApi::class)
                Json.encodeToStream(action, FileOutputStream(file, true))
                file.appendText("\n")
            } catch (e: IOException) {
                logger.error("Failed to write to crash file", e)
            }
        }
    }

    override fun record(crash: Crash.UncaughtExceptionCrash) {
        recordCrashAction(GleanCrashAction.Count(UNCAUGHT_EXCEPTION_KEY))
        recordCrashAction(
            GleanCrashAction.Ping(
                uptimeNanos = uptime(),
                processType = "main",
                remoteType = "",
                timeMillis = crash.timestamp,
                startup = false,
                reason = Pings.crashReasonCodes.crash,
                cause = "java_exception",
            ),
        )
    }

    override fun record(crash: Crash.NativeCodeCrash) {
        when (crash.processType) {
            Crash.NativeCodeCrash.PROCESS_TYPE_MAIN ->
                recordCrashAction(GleanCrashAction.Count(MAIN_PROCESS_NATIVE_CODE_CRASH_KEY))
            Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD ->
                recordCrashAction(
                    GleanCrashAction.Count(
                        FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY,
                    ),
                )
            Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD ->
                recordCrashAction(
                    GleanCrashAction.Count(
                        BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY,
                    ),
                )
        }

        // The `processType` property on a crash is a bit confusing because it does not map to the actual process types
        // (like main, content, gpu, etc.). This property indicates what UI we should show to users given that "main"
        // crashes essentially kill the app, "foreground child" crashes are likely tab crashes, and "background child"
        // crashes are occurring in other processes (like GPU and extensions) for which users shouldn't notice anything
        // (because there shouldn't be any noticeable impact in the app and the processes will be recreated
        // automatically).
        val processType = when (crash.processType) {
            Crash.NativeCodeCrash.PROCESS_TYPE_MAIN -> "main"

            Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD -> {
                when (crash.remoteType) {
                    // The extensions process is a content process as per:
                    // https://firefox-source-docs.mozilla.org/dom/ipc/process_model.html#webextensions
                    "extension" -> "content"

                    else -> "utility"
                }
            }

            Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD -> "content"

            else -> "main"
        }
        recordCrashAction(
            GleanCrashAction.Ping(
                uptimeNanos = uptime(),
                processType = processType,
                remoteType = crash.remoteType ?: "",
                timeMillis = crash.timestamp,
                startup = false,
                reason = Pings.crashReasonCodes.crash,
                cause = "os_fault",
            ),
        )
    }

    override fun record(throwable: Throwable) {
        recordCrashAction(GleanCrashAction.Count(CAUGHT_EXCEPTION_KEY))
    }
}

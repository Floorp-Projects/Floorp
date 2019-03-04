/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.support.annotation.StyleRes
import android.support.annotation.VisibleForTesting
import mozilla.components.lib.crash.handler.ExceptionHandler
import mozilla.components.lib.crash.prompt.CrashReporterActivity
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.support.base.log.logger.Logger

/**
 * A generic crash reporter that can report crashes to multiple services.
 *
 * In the `onCreate()` method of your Application class create a `CrashReporter` instance and call `install()`:
 *
 * ```Kotlin
 *   CrashReporter(
 *     services = listOf(
 *       // List the crash reporting services you want to use
 *     )
 *   ).install(this)
 * ```
 *
 * With this minimal setup the crash reporting library will capture "uncaught exception" crashes and "native code"
 * crashes and forward them to the configured crash reporting services.
 *
 * @property enabled Enable/Disable crash reporting.
 *
 * @param services List of crash reporting services that should receive crash reports.
 * @param shouldPrompt Whether or not the user should be prompted to confirm sending crash reports.
 * @param enabled Enable/Disable crash reporting.
 * @param promptConfiguration Configuration for customizing the crash reporter prompt.
 * @param nonFatalCrashIntent A [PendingIntent] that will be launched if a non fatal crash (main process not affected)
 *                            happened. This gives the app the opportunity to show an in-app confirmation UI before
 *                            sending a crash report. See component README for details.
 */
class CrashReporter(
    private val services: List<CrashReporterService>,
    private val shouldPrompt: Prompt = Prompt.NEVER,
    var enabled: Boolean = true,
    internal val promptConfiguration: PromptConfiguration = PromptConfiguration(),
    private val nonFatalCrashIntent: PendingIntent? = null
) {
    internal val logger = Logger("mozac/CrashReporter")

    init {
        if (services.isEmpty()) {
            throw IllegalArgumentException("No crash reporter services defined")
        }
    }

    /**
     * Install this [CrashReporter] instance. At this point the component will be setup to collect crash reports.
     */
    fun install(applicationContext: Context): CrashReporter {
        CrashReporter.instance = this

        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
        val handler = ExceptionHandler(applicationContext, this, defaultHandler)
        Thread.setDefaultUncaughtExceptionHandler(handler)

        return this
    }

    /**
     * Submit a crash report to all registered services.
     *
     * Note: This method may block and perform I/O on the calling thread.
     */
    fun submitReport(crash: Crash) {
        services.forEach { service ->
            when (crash) {
                is Crash.NativeCodeCrash -> service.report(crash)
                is Crash.UncaughtExceptionCrash -> service.report(crash)
            }
        }

        logger.info("Crash report submitted to ${services.size} services")
    }

    internal fun onCrash(context: Context, crash: Crash) {
        if (!enabled) {
            return
        }

        logger.info("Received crash: $crash")

        if (shouldPromptForCrash(crash)) {
            if (crash is Crash.NativeCodeCrash &&
                !crash.isFatal &&
                nonFatalCrashIntent != null) {
                // This crash was not fatal and the app has registered a pending intent: Send Intent to app and let the
                // app handle showing a confirmation UI.
                logger.info("Invoking non-fatal PendingIntent")

                val additionalIntent = Intent()
                crash.fillIn(additionalIntent)
                nonFatalCrashIntent.send(context, 0, additionalIntent)
            } else {
                logger.info("Showing prompt")

                showPrompt(context, crash)
            }
        } else {
            logger.info("Immediately submitting crash report")

            submitReport(crash)
        }
    }

    internal fun showPrompt(context: Context, crash: Crash) {
        val intent = Intent(context, CrashReporterActivity::class.java)
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK)
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION)

        crash.fillIn(intent)

        context.startActivity(intent)
    }

    private fun shouldPromptForCrash(crash: Crash): Boolean {
        return when (shouldPrompt) {
            Prompt.ALWAYS -> true
            Prompt.NEVER -> false
            Prompt.ONLY_NATIVE_CRASH -> crash is Crash.NativeCodeCrash
        }
    }

    enum class Prompt {
        /**
         * Never prompt the user. Always submit crash reports immediately.
         */
        NEVER,

        /**
         * Only prompt the user for native code crashes.
         */
        ONLY_NATIVE_CRASH,

        /**
         * Always prompt the user for confirmation before sending crash reports.
         */
        ALWAYS
    }

    /**
     * Configuration for the crash reporter prompt.
     */
    data class PromptConfiguration(
        internal val appName: String = "App",
        internal val organizationName: String = "Mozilla",
        internal val message: String? = null,
        @StyleRes internal val theme: Int = R.style.Theme_Mozac_CrashReporter
    )

    companion object {
        @Volatile
        private var instance: CrashReporter? = null

        @VisibleForTesting internal fun reset() {
            instance = null
        }

        internal val requireInstance: CrashReporter
            get() = instance ?: throw IllegalStateException(
                "You need to call install() on your CrashReporter instance from Application.onCreate().")
    }
}

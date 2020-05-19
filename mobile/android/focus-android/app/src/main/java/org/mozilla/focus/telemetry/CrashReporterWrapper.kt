/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.telemetry

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.lib.crash.service.MozillaSocorroService
import mozilla.components.lib.crash.service.SentryService
import org.mozilla.focus.BuildConfig
import org.mozilla.focus.R
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.locale.LocaleManager
import org.mozilla.geckoview.BuildConfig.MOZ_APP_BUILDID
import org.mozilla.geckoview.BuildConfig.MOZ_APP_VENDOR
import org.mozilla.geckoview.BuildConfig.MOZ_APP_VERSION
import org.mozilla.geckoview.BuildConfig.MOZ_UPDATE_CHANNEL
import java.util.Locale

/**
 * An interface to the Sentry crash reporting SDK. All code that touches the Sentry APIs
 * directly should go in here (like TelemetryWrapper).
 *
 * With the current implementation, to enable Sentry on Beta/Release builds, add a
 * <project-dir>/.sentry_token file with your DSN. To enable Sentry on Debug
 * builds, add a .sentry_token file and replace the [TelemetryWrapper.isTelemetryEnabled]
 * value with true (upload is disabled by default in dev builds).
 */

object CrashReporterWrapper {
    private const val SOCORRO_APP_NAME = "Focus"
    private const val TAG_BUILD_FLAVOR: String = "build_flavor"
    private const val TAG_BUILD_TYPE: String = "build_type"
    private const val TAG_LOCALE_LANG_TAG: String = "locale_lang_tag"
    private var crashReporter: CrashReporter? = null

    fun init(context: Context) {
        // The BuildConfig value is populated from a file at compile time.
        // If the file did not exist, the value will be null.
        val supportedBuild = Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
        if (!supportedBuild) return

        val services = mutableListOf<CrashReporterService>()

        if (isSentryEnabled()) {
            val sentryService = SentryService(
                context,
                BuildConfig.SENTRY_TOKEN,
                tags = createTags(context),
                environment = BuildConfig.BUILD_TYPE,
                sendEventForNativeCrashes = false // Do not send native crashes to Sentry
            )

            services.add(sentryService)
        }

        val socorroService = MozillaSocorroService(
            context,
            appName = SOCORRO_APP_NAME,
            version = MOZ_APP_VERSION,
            buildId = MOZ_APP_BUILDID,
            vendor = MOZ_APP_VENDOR,
            releaseChannel = MOZ_UPDATE_CHANNEL
        )
        services.add(socorroService)

        val intent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
        }

        val pendingIntent = PendingIntent.getActivity(
                context,
                0,
                intent,
                0)

        crashReporter = CrashReporter(
                context = context,
                services = services,
                promptConfiguration = CrashReporter.PromptConfiguration(
                        appName = context.resources.getString(R.string.app_name)
                ),
                shouldPrompt = CrashReporter.Prompt.ALWAYS,
                nonFatalCrashIntent = pendingIntent).install(context)

        onIsEnabledChanged(context)
    }

    fun isSentryEnabled() = !BuildConfig.SENTRY_TOKEN.isNullOrEmpty()

    fun onIsEnabledChanged(context: Context, isEnabled: Boolean = TelemetryWrapper.isTelemetryEnabled(context)) {
        crashReporter?.enabled = isEnabled
    }

    fun submitCrash(crash: Crash) {
        GlobalScope.launch(IO) {
            crashReporter?.submitReport(crash)
        }
    }

    private fun createTags(context: Context) = mapOf(
            TAG_BUILD_FLAVOR to BuildConfig.FLAVOR,
            TAG_BUILD_TYPE to BuildConfig.BUILD_TYPE,
            TAG_LOCALE_LANG_TAG to getLocaleTag(context))

    private fun getLocaleTag(context: Context): String {
        val currentLocale = LocaleManager.getInstance().getCurrentLocale(context)
        return if (currentLocale != null) {
            currentLocale.toLanguageTag()
        } else {
            Locale.getDefault().toLanguageTag()
        }
    }
}

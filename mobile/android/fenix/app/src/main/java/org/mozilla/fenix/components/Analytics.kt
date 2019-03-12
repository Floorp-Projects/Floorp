/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.app.Application
import android.content.Context
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.MozillaSocorroService
import mozilla.components.lib.crash.service.SentryService
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.GleanMetricsService
import org.mozilla.fenix.components.metrics.LeanplumMetricsService
import org.mozilla.fenix.components.metrics.Metrics
import org.mozilla.geckoview.BuildConfig.MOZ_APP_BUILDID
import org.mozilla.geckoview.BuildConfig.MOZ_APP_VERSION

/**
 * Component group for all functionality related to analytics e.g. crash reporting and telemetry.
 */
class Analytics(
    private val context: Context
) {
    val crashReporter: CrashReporter by lazy {
        val sentryService = SentryService(
            context,
            BuildConfig.SENTRY_TOKEN,
            tags = mapOf("geckoview" to "$MOZ_APP_VERSION-$MOZ_APP_BUILDID"),
            sendEventForNativeCrashes = true
        )

        val socorroService = MozillaSocorroService(context, "Fenix")

        CrashReporter(
            services = listOf(sentryService, socorroService),
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            promptConfiguration = CrashReporter.PromptConfiguration(
                appName = context.getString(R.string.app_name),
                organizationName = "Mozilla"
            ),
            enabled = true
        )
    }

    val metrics: Metrics by lazy {
        Metrics(
            listOf(
                GleanMetricsService(context),
                LeanplumMetricsService(context as Application)
            )
        )
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.experiments

import android.content.Context
import androidx.core.net.toUri
import kotlinx.coroutines.runBlocking
import mozilla.components.service.nimbus.Nimbus
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.service.nimbus.NimbusAppInfo
import mozilla.components.service.nimbus.NimbusDisabled
import mozilla.components.service.nimbus.NimbusServerSettings
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.experiments.nimbus.NimbusInterface
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment
import org.mozilla.experiments.nimbus.internal.NimbusException
import org.mozilla.experiments.nimbus.joinOrTimeout
import org.mozilla.focus.BuildConfig
import org.mozilla.focus.GleanMetrics.NimbusExperiments
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import org.mozilla.focus.nimbus.FocusNimbus

private const val TIME_OUT_LOADING_EXPERIMENT_FROM_DISK_MS = 200L

@Suppress("TooGenericExceptionCaught")
fun createNimbus(context: Context, url: String?): NimbusApi {
    val errorReporter: ((String, Throwable) -> Unit) = reporter@{ message, e ->
        Logger.error("Nimbus error: $message", e)

        if (e is NimbusException && !e.isReportableError()) {
            return@reporter
        }

        context.components.crashReporter.submitCaughtException(e)
    }

    // Eventually we'll want to use `NimbusDisabled` when we have no NIMBUS_ENDPOINT.
    // but we keep this here to not mix feature flags and how we configure Nimbus.
    val serverSettings = if (!url.isNullOrBlank()) {
        if (context.settings.shouldUseNimbusPreview) {
            NimbusServerSettings(url = url.toUri(), collection = "nimbus-preview")
        } else {
            NimbusServerSettings(url = url.toUri())
        }
    } else {
        null
    }

    val isTheFirstLaunch = context.settings.getAppLaunchCount() == 0

    // The name "focus-android" or "klar-android" here corresponds to the app_name defined
    // for the family of apps that encompasses all of the channels for the Focus app.
    // This is defined upstream in the telemetry system. For more context on where the
    // app_name come from see:
    // https://probeinfo.telemetry.mozilla.org/v2/glean/app-listings
    // and
    // https://github.com/mozilla/probe-scraper/blob/master/repositories.yaml
    val appInfo = NimbusAppInfo(
        appName = getNimbusAppName(),
        // Note: Using BuildConfig.BUILD_TYPE is important here so that it matches the value
        // passed into Glean. `Config.channel.toString()` turned out to be non-deterministic
        // and would mostly produce the value `Beta` and rarely would produce `beta`.
        channel = BuildConfig.BUILD_TYPE,
    )
    return try {
        Nimbus(context, appInfo, serverSettings, errorReporter).apply {
            register(EventsObserver)

            val job = if (isTheFirstLaunch || url.isNullOrBlank()) {
                applyLocalExperiments(R.raw.initial_experiments)
            } else {
                applyPendingExperiments()
            }

            runBlocking {
                job.joinOrTimeout(TIME_OUT_LOADING_EXPERIMENT_FROM_DISK_MS)
            }
        }
    } catch (e: Throwable) {
        // Something went wrong. We'd like not to, but stability of the app is more important than
        // failing fast here.
        errorReporter("Failed to initialize Nimbus", e)
        NimbusDisabled(context = context)
    }
}

internal fun finishNimbusInitialization(experiments: NimbusApi) =
    experiments.run {
        // We fetch experiments in all cases,
        if (context.settings.getAppLaunchCount() == 0) {
            // … however on first run, we immediately apply pending experiments.
            // We also want to measure how long this will take, with Glean.
            register(
                object : NimbusInterface.Observer {
                    override fun onExperimentsFetched() {
                        NimbusExperiments.nimbusInitialFetch.stop()
                        applyPendingExperiments()
                        // Remove lingering observer when we're done fetching experiments on startup.
                        unregister(this)
                    }
                },
            )
            NimbusExperiments.nimbusInitialFetch.start()
        }
        fetchExperiments()
    }

fun getNimbusAppName(): String {
    return if (BuildConfig.FLAVOR.contains("focus")) {
        "focus_android"
    } else {
        "klar_android"
    }
}

/**
 * Classifies which errors we should forward to our crash reporter or not. We want to filter out the
 * non-reportable ones if we know there is no reasonable action that we can perform.
 *
 * This fix should be upstreamed as part of: https://github.com/mozilla/application-services/issues/4333
 */
fun NimbusException.isReportableError(): Boolean {
    return when (this) {
        is NimbusException.RequestException,
        is NimbusException.ResponseException,
        -> false
        else -> true
    }
}

/**
 * Focus specific observer of Nimbus events.
 *
 * The generated code `FocusNimbus` provides a cache which should be invalidated
 * when the experiments recipes are updated.
 */
private object EventsObserver : NimbusInterface.Observer {
    override fun onUpdatesApplied(updated: List<EnrolledExperiment>) {
        FocusNimbus.invalidateCachedValues()
    }
}

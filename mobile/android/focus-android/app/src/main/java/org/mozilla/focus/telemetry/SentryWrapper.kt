/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.telemetry

import android.content.Context
import io.sentry.Sentry
import io.sentry.android.AndroidSentryClientFactory
import org.mozilla.focus.BuildConfig

/**
 * An interface to the Sentry crash reporting SDK. All code that touches the Sentry APIs
 * directly should go in here (like TelemetryWrapper).
 *
 * With the current implementation, to enable Sentry on Beta/Release builds, add a
 * <project-dir>/.sentry_token file with your DSN. To enable Sentry on Debug
 * builds, add a .sentry_token file and replace the [TelemetryWrapper.isTelemetryEnabled]
 * value with true (upload is disabled by default in dev builds).
 */
object SentryWrapper {

    private const val TAG_BUILD_FLAVOR: String = "build_flavor"
    private const val TAG_BUILD_TYPE: String = "build_type"

    fun init(context: Context) {
        onIsEnabledChanged(context, TelemetryWrapper.isTelemetryEnabled(context))
        addTags()
    }

    internal fun onIsEnabledChanged(context: Context, isEnabled: Boolean) {
        // The BuildConfig value is populated from a file at compile time.
        // If the file did not exist, the value will be null.
        //
        // If you provide a null DSN to Sentry, it will disable upload and buffering to disk:
        // https://github.com/getsentry/sentry-java/issues/574#issuecomment-378298484
        //
        // In the current implementation, each time `init` is called, it will overwrite the
        // stored client and DSN, thus calling it with a null DSN will have the affect of
        // disabling the client: https://github.com/getsentry/sentry-java/issues/574#issuecomment-378406105
        val sentryDsn = if (isEnabled) BuildConfig.SENTRY_TOKEN else null
        Sentry.init(sentryDsn, AndroidSentryClientFactory(context.applicationContext))
    }

    private fun addTags() {
        Sentry.getContext().addTag(TAG_BUILD_FLAVOR, BuildConfig.FLAVOR)
        Sentry.getContext().addTag(TAG_BUILD_TYPE, BuildConfig.BUILD_TYPE)
    }

    fun captureGeckoCrash() {
        Sentry.capture("GeckoSession crashes, opening new session")
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import android.os.Bundle
import mozilla.components.browser.engine.gecko.autofill.GeckoLoginDelegateWrapper
import mozilla.components.browser.engine.gecko.glean.GeckoAdapter
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.lib.crash.handler.CrashHandlerService
import mozilla.components.service.sync.logins.GeckoLoginStorageDelegate
import org.mozilla.fenix.Config
import org.mozilla.fenix.ext.components
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings

object GeckoProvider {
    var testConfig: Bundle? = null
    private var runtime: GeckoRuntime? = null

    @Synchronized
    fun getOrCreateRuntime(
        context: Context,
        storage: Lazy<LoginsStorage>,
        trackingProtectionPolicy: TrackingProtectionPolicy
    ): GeckoRuntime {
        if (runtime == null) {
            runtime = createRuntime(context, storage, trackingProtectionPolicy)
        }

        return runtime!!
    }

    private fun createRuntime(
        context: Context,
        storage: Lazy<LoginsStorage>,
        policy: TrackingProtectionPolicy
    ): GeckoRuntime {
        val builder = GeckoRuntimeSettings.Builder()

        testConfig?.let {
            builder.extras(it)
                .remoteDebuggingEnabled(true)
        }

        // Use meeee.
        policy.hashCode()

        val runtimeSettings = builder
            .crashHandler(CrashHandlerService::class.java)
            .telemetryDelegate(GeckoAdapter())
            // TODO: Fix me!
            // .contentBlocking(policy.toContentBlockingSetting())
            .aboutConfigEnabled(Config.channel.isBeta)
            .debugLogging(Config.channel.isDebug)
            .build()

        val settings = context.components.settings
        if (!settings.shouldUseAutoSize) {
            runtimeSettings.automaticFontSizeAdjustment = false
            val fontSize = settings.fontSizeFactor
            runtimeSettings.fontSizeFactor = fontSize
        }

        val geckoRuntime = GeckoRuntime.create(context, runtimeSettings)
        val loginStorageDelegate = GeckoLoginStorageDelegate(storage)
        geckoRuntime.loginStorageDelegate = GeckoLoginDelegateWrapper(loginStorageDelegate)

        return geckoRuntime
    }
}

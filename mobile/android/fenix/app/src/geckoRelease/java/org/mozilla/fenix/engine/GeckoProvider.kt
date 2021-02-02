/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import android.os.Bundle
import mozilla.components.browser.engine.gecko.autofill.GeckoLoginDelegateWrapper
import mozilla.components.browser.engine.gecko.ext.toContentBlockingSetting
import mozilla.components.browser.engine.gecko.glean.GeckoAdapter
import mozilla.components.concept.engine.EngineSession.TrackingProtectionPolicy
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.lib.crash.handler.CrashHandlerService
import mozilla.components.service.sync.logins.GeckoLoginStorageDelegate
import org.mozilla.fenix.Config
import org.mozilla.fenix.ext.components
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings
import org.mozilla.geckoview.ContentBlocking.SafeBrowsingProvider

object GeckoProvider {
    var testConfig: Bundle? = null
    private var runtime: GeckoRuntime? = null
    const val CN_UPDATE_URL =
        "https://sb.firefox.com.cn/downloads?client=SAFEBROWSING_ID&appver=%MAJOR_VERSION%&pver=2.2"
    const val CN_GET_HASH_URL =
        "https://sb.firefox.com.cn/gethash?client=SAFEBROWSING_ID&appver=%MAJOR_VERSION%&pver=2.2"

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
            .contentBlocking(policy.toContentBlockingSetting())
            .aboutConfigEnabled(Config.channel.isBeta)
            .debugLogging(Config.channel.isDebug)
            .build()

        val settings = context.components.settings
        if (!settings.shouldUseAutoSize) {
            runtimeSettings.automaticFontSizeAdjustment = false
            val fontSize = settings.fontSizeFactor
            runtimeSettings.fontSizeFactor = fontSize
        }

        // Add safebrowsing providers for China
        if (Config.channel.isMozillaOnline) {
            val mozcn = SafeBrowsingProvider
                .withName("mozcn")
                .version("2.2")
                .lists("m6eb-phish-shavar", "m6ib-phish-shavar")
                .updateUrl(CN_UPDATE_URL)
                .getHashUrl(CN_GET_HASH_URL)
                .build()

            runtimeSettings.contentBlocking.setSafeBrowsingProviders(mozcn,
                // Keep the existing configuration
                ContentBlocking.GOOGLE_SAFE_BROWSING_PROVIDER,
                ContentBlocking.GOOGLE_LEGACY_SAFE_BROWSING_PROVIDER)

            runtimeSettings.contentBlocking.setSafeBrowsingPhishingTable(
                "m6eb-phish-shavar",
                "m6ib-phish-shavar",
                // Existing configuration
                "goog-phish-proto")
        }

        val geckoRuntime = GeckoRuntime.create(context, runtimeSettings)
        val loginStorageDelegate = GeckoLoginStorageDelegate(storage)
        geckoRuntime.loginStorageDelegate = GeckoLoginDelegateWrapper(loginStorageDelegate)

        return geckoRuntime
    }
}

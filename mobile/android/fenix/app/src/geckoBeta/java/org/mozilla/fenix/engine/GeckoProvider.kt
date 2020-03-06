/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context
import android.os.Bundle
import mozilla.components.browser.engine.gecko.autofill.GeckoLoginDelegateWrapper
import mozilla.components.browser.engine.gecko.glean.GeckoAdapter
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.lib.crash.handler.CrashHandlerService
import mozilla.components.service.experiments.Experiments
import mozilla.components.service.sync.logins.GeckoLoginStorageDelegate
import org.mozilla.fenix.Config
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings

object GeckoProvider {
    var testConfig: Bundle? = null
    private var runtime: GeckoRuntime? = null

    @Synchronized
    fun getOrCreateRuntime(
        context: Context,
        storage: LoginsStorage
    ): GeckoRuntime {
        if (runtime == null) {
            runtime = createRuntime(context, storage)
        }

        return runtime!!
    }

    private fun createRuntime(
        context: Context,
        storage: LoginsStorage
    ): GeckoRuntime {
        val builder = GeckoRuntimeSettings.Builder()

        testConfig?.let {
            builder.extras(it)
                .remoteDebuggingEnabled(true)
        }

        val runtimeSettings = builder
            .crashHandler(CrashHandlerService::class.java)
            .useContentProcessHint(true)
            .telemetryDelegate(GeckoAdapter())
            .aboutConfigEnabled(Config.channel.isBeta)
            .debugLogging(Config.channel.isDebug)
            .build()

        Experiments.withExperiment("webrender-performance-comparison-experiment") { branchName ->
            if (branchName == "disable_webrender") {
                runtimeSettings.extras.putInt("forcedisablewebrender", 1)
            }
        }

        if (!Settings.getInstance(context).shouldUseAutoSize) {
            runtimeSettings.automaticFontSizeAdjustment = false
            val fontSize = Settings.getInstance(context).fontSizeFactor
            runtimeSettings.fontSizeFactor = fontSize
        }

        val geckoRuntime = GeckoRuntime.create(context, runtimeSettings)
        // As a quick fix for #8967 we are conflating "should autofill" with "should save logins"
        val loginStorageDelegate = GeckoLoginStorageDelegate(
            storage,
            { context.settings().shouldPromptToSaveLogins }
        )
        geckoRuntime.loginStorageDelegate = GeckoLoginDelegateWrapper(loginStorageDelegate)

        return geckoRuntime
    }
}

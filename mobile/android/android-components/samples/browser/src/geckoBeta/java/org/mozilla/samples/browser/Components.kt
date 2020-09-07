/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Context
import mozilla.components.browser.engine.gecko.GeckoEngine
import mozilla.components.browser.engine.gecko.glean.GeckoAdapter
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.webcompat.WebCompatFeature
import mozilla.components.feature.webcompat.reporter.WebCompatReporterFeature
import mozilla.components.lib.crash.handler.CrashHandlerService
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoRuntimeSettings

/**
 * Helper class for lazily instantiating components needed by the application.
 */
class Components(applicationContext: Context) : DefaultComponents(applicationContext) {
    private val runtime by lazy {
        // Allow for exfiltrating Gecko metrics through the Glean SDK.
        val builder = GeckoRuntimeSettings.Builder()
        builder.crashHandler(CrashHandlerService::class.java)
        builder.telemetryDelegate(GeckoAdapter())
        GeckoRuntime.create(applicationContext, builder.build())
    }

    override val engine: Engine by lazy {
        GeckoEngine(applicationContext, engineSettings, runtime).also {
            WebCompatFeature.install(it)
            WebCompatReporterFeature.install(it)
        }
    }

    override val client by lazy { GeckoViewFetchClient(applicationContext, runtime) }
}

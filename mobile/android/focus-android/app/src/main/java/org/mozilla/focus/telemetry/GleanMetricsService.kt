/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.feature.search.ext.waitForSelectedOrDefaultSearchEngine
import mozilla.components.service.glean.net.ConceptFetchHttpUploader
import mozilla.telemetry.glean.Glean
import mozilla.telemetry.glean.config.Configuration
import org.mozilla.focus.BuildConfig
import org.mozilla.focus.Components
import org.mozilla.focus.GleanMetrics.Browser
import org.mozilla.focus.GleanMetrics.GleanBuildInfo
import org.mozilla.focus.GleanMetrics.LegacyIds
import org.mozilla.focus.GleanMetrics.Pings
import org.mozilla.focus.ext.components
import org.mozilla.focus.telemetry.TelemetryWrapper.isTelemetryEnabled
import org.mozilla.focus.utils.Settings
import java.util.UUID

/**
 * Glean telemetry service.
 *
 * To track events, use Glean's generated bindings directly.
 */
class GleanMetricsService(context: Context) : MetricsService {

    @Suppress("UnusedPrivateMember")
    private val activationPing = ActivationPing(context)

    @OptIn(DelicateCoroutinesApi::class)
    override fun initialize(context: Context) {
        val components = context.components
        val settings = Settings.getInstance(context)
        val telemetryEnabled = isTelemetryEnabled(context)

        Glean.initialize(
            applicationContext = context,
            uploadEnabled = telemetryEnabled,
            configuration = Configuration(
                channel = BuildConfig.FLAVOR,
                httpClient = ConceptFetchHttpUploader(
                    client = lazy(LazyThreadSafetyMode.NONE) { components.client },
                    usePrivateRequest = true
                )
            ),
            buildInfo = GleanBuildInfo.buildInfo
        )

        Glean.registerPings(Pings)

        if (telemetryEnabled) {
            installSearchTelemetryExtensions(components)
        }

        // Do this immediately after init.
        GlobalScope.launch(IO) {

            // Wait for preferences to be collected before we send the activation ping.
            collectPrefMetrics(components, settings).await()

            // Set the client ID in Glean as part of the deletion-request.
            LegacyIds.clientId.set(UUID.fromString(TelemetryWrapper.clientId))

            components.store.waitForSelectedOrDefaultSearchEngine { searchEngine ->
                if (searchEngine != null) {
                    Browser.defaultSearchEngine.set(getDefaultSearchEngineIdentifierForTelemetry(context))
                }

                // Disabled until data-review r+
                // activationPing.checkAndSend()
            }
        }
    }

    private fun collectPrefMetrics(
        components: Components,
        settings: Settings
    ) = CoroutineScope(IO).async {
        Browser.isDefault.set(settings.isDefaultBrowser())
        Browser.localeOverride.set(components.store.state.locale?.displayName ?: "none")
    }

    private fun getDefaultSearchEngineIdentifierForTelemetry(context: Context): String {
        val searchEngine = context.components.store.state.search.selectedOrDefaultSearchEngine
        return if (searchEngine?.type == SearchEngine.Type.CUSTOM) {
            "custom"
        } else {
            searchEngine?.name ?: "<none>"
        }
    }

    @VisibleForTesting
    internal fun installSearchTelemetryExtensions(components: Components) {
        val engine = components.engine
        components.store.apply {
            components.adsTelemetry.install(engine, this)
            components.searchTelemetry.install(engine, this)
        }
    }
}

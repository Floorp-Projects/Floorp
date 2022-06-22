/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.content.Context
import android.os.Build
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
import org.mozilla.focus.GleanMetrics.MozillaProducts
import org.mozilla.focus.GleanMetrics.Pings
import org.mozilla.focus.GleanMetrics.Preferences
import org.mozilla.focus.GleanMetrics.Shortcuts
import org.mozilla.focus.GleanMetrics.TrackingProtection
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import org.mozilla.focus.telemetry.TelemetryWrapper.isTelemetryEnabled
import org.mozilla.focus.topsites.DefaultTopSitesStorage.Companion.TOP_SITES_MAX_LIMIT
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
        val settings = context.settings
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
            collectPrefMetrics(components, settings, context).await()

            // Set the client ID in Glean as part of the deletion-request.
            LegacyIds.clientId.set(UUID.fromString(TelemetryWrapper.clientId))

            components.store.waitForSelectedOrDefaultSearchEngine { searchEngine ->
                if (searchEngine != null) {
                    Browser.defaultSearchEngine.set(getDefaultSearchEngineIdentifierForTelemetry(context))
                }

                activationPing.checkAndSend()
            }
        }
    }

    private fun collectPrefMetrics(
        components: Components,
        settings: Settings,
        context: Context
    ) = CoroutineScope(IO).async {
        val installedBrowsers = BrowsersCache.all(context)
        val hasFenixInstalled = FenixProductDetector.getInstalledFenixVersions(context).isNotEmpty()
        val isFenixDefaultBrowser = FenixProductDetector.isFenixDefaultBrowser(installedBrowsers.defaultBrowser)
        val isFocusDefaultBrowser = installedBrowsers.isDefaultBrowser

        Browser.isDefault.set(isFocusDefaultBrowser)
        Browser.localeOverride.set(components.store.state.locale?.displayName ?: "none")
        val shortcutsOnHomeNumber = components.topSitesStorage.getTopSites(
            totalSites = TOP_SITES_MAX_LIMIT,
            frecencyConfig = null
        ).size
        Shortcuts.shortcutsOnHomeNumber.set(shortcutsOnHomeNumber.toLong())

        val installSourcePackage = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            context.packageManager.getInstallSourceInfo(context.packageName).installingPackageName
        } else {
            @Suppress("DEPRECATION")
            context.packageManager.getInstallerPackageName(context.packageName)
        }

        Browser.installSource.set(installSourcePackage.orEmpty())

        // Fenix telemetry
        MozillaProducts.hasFenixInstalled.set(hasFenixInstalled)
        MozillaProducts.isFenixDefaultBrowser.set(isFenixDefaultBrowser)

        // tracking protection metrics
        TrackingProtection.hasAdvertisingBlocked.set(settings.hasAdvertisingBlocked())
        TrackingProtection.hasAnalyticsBlocked.set(settings.hasAnalyticsBlocked())
        TrackingProtection.hasContentBlocked.set(settings.shouldBlockOtherTrackers())
        TrackingProtection.hasSocialBlocked.set(settings.hasSocialBlocked())

        // theme telemetry
        val currentTheme =
            when {
                settings.lightThemeSelected -> {
                    "Light"
                }
                settings.darkThemeSelected -> {
                    "Dark"
                }

                settings.useDefaultThemeSelected -> {
                    "Follow device"
                }
                else -> ""
            }
        if (currentTheme.isNotEmpty()) {
            Preferences.userTheme.set(currentTheme)
        }
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

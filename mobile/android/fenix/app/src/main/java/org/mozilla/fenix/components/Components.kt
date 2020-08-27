/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.content.Intent
import androidx.core.content.getSystemService
import androidx.core.net.toUri
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker
import mozilla.components.feature.addons.migration.SupportedAddonsChecker
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.DefaultAddonUpdater
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.migration.state.MigrationStore
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.metrics.AppStartupTelemetry
import org.mozilla.fenix.utils.ClipboardHandler
import org.mozilla.fenix.utils.Mockable
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.wifi.WifiConnectionMonitor
import java.util.concurrent.TimeUnit

private const val DAY_IN_MINUTES = 24 * 60L

/**
 * Provides access to all components.
 */
@Mockable
class Components(private val context: Context) {
    val backgroundServices by lazy {
        BackgroundServices(
            context,
            push,
            analytics.crashReporter,
            core.lazyHistoryStorage,
            core.lazyBookmarksStorage,
            core.lazyPasswordsStorage,
            core.lazyRemoteTabsStorage
        )
    }
    val services by lazy { Services(context, backgroundServices.accountManager) }
    val core by lazy { Core(context, analytics.crashReporter) }
    val search by lazy { Search(context) }
    val useCases by lazy {
        UseCases(
            context,
            core.engine,
            core.sessionManager,
            core.store,
            search.searchEngineManager,
            core.webAppShortcutManager,
            core.topSiteStorage
        )
    }
    val intentProcessors by lazy {
        IntentProcessors(
            context,
            core.sessionManager,
            useCases.sessionUseCases,
            useCases.searchUseCases,
            core.relationChecker,
            core.customTabsStore,
            migrationStore,
            core.webAppManifestStorage
        )
    }

    val addonCollectionProvider by lazy {
        if (!BuildConfig.AMO_COLLECTION.isNullOrEmpty()) {
            AddonCollectionProvider(
                context,
                core.client,
                collectionName = BuildConfig.AMO_COLLECTION,
                maxCacheAgeInMinutes = DAY_IN_MINUTES
            )
        } else {
            AddonCollectionProvider(context, core.client, maxCacheAgeInMinutes = DAY_IN_MINUTES)
        }
    }

    val appStartupTelemetry by lazy { AppStartupTelemetry(analytics.metrics) }

    @Suppress("MagicNumber")
    val addonUpdater by lazy {
        DefaultAddonUpdater(context, AddonUpdater.Frequency(12, TimeUnit.HOURS))
    }

    @Suppress("MagicNumber")
    val supportedAddonsChecker by lazy {
        DefaultSupportedAddonsChecker(context, SupportedAddonsChecker.Frequency(12, TimeUnit.HOURS),
            onNotificationClickIntent = Intent(context, HomeActivity::class.java).apply {
                action = Intent.ACTION_VIEW
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                data = "fenix://settings_addon_manager".toUri()
            }
        )
    }

    val addonManager by lazy {
        AddonManager(core.store, core.engine, addonCollectionProvider, addonUpdater)
    }

    val analytics by lazy { Analytics(context) }
    val publicSuffixList by lazy { PublicSuffixList(context) }
    val clipboardHandler by lazy { ClipboardHandler(context) }
    val migrationStore by lazy { MigrationStore() }
    val performance by lazy { PerformanceComponent() }
    val push by lazy { Push(context, analytics.crashReporter) }
    val wifiConnectionMonitor by lazy { WifiConnectionMonitor(context.getSystemService()!!) }

    val settings by lazy { Settings(context) }

    val reviewPromptController by lazy {
        ReviewPromptController(
            context,
            FenixReviewSettings(settings)
        )
    }
}

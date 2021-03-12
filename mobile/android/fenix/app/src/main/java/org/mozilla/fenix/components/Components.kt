/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.app.Application
import android.content.Context
import android.content.Intent
import androidx.core.net.toUri
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.migration.DefaultSupportedAddonsChecker
import mozilla.components.feature.addons.migration.SupportedAddonsChecker
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.DefaultAddonUpdater
import mozilla.components.feature.autofill.AutofillConfiguration
import mozilla.components.feature.sitepermissions.SitePermissionsStorage
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.migration.state.MigrationStore
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.autofill.AutofillUnlockActivity
import org.mozilla.fenix.perf.StrictModeManager
import org.mozilla.fenix.components.metrics.AppStartupTelemetry
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.perf.lazyMonitored
import org.mozilla.fenix.utils.ClipboardHandler
import org.mozilla.fenix.utils.Mockable
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.wifi.WifiConnectionMonitor
import java.util.concurrent.TimeUnit

private const val AMO_COLLECTION_MAX_CACHE_AGE = 2 * 24 * 60L // Two days in minutes

/**
 * Provides access to all components. This class is an implementation of the Service Locator
 * pattern, which helps us manage the dependencies in our app.
 *
 * Note: these aren't just "components" from "android-components": they're any "component" that
 * can be considered a building block of our app.
 */
@Mockable
class Components(private val context: Context) {
    val backgroundServices by lazyMonitored {
        BackgroundServices(
            context,
            push,
            analytics.crashReporter,
            core.lazyHistoryStorage,
            core.lazyBookmarksStorage,
            core.lazyPasswordsStorage,
            core.lazyRemoteTabsStorage,
            strictMode
        )
    }
    val services by lazyMonitored { Services(context, backgroundServices.accountManager) }
    val core by lazyMonitored { Core(context, analytics.crashReporter, strictMode) }
    @Suppress("Deprecation")
    val useCases by lazyMonitored {
        UseCases(
            context,
            core.engine,
            core.sessionManager,
            core.store,
            core.webAppShortcutManager,
            core.topSitesStorage
        )
    }

    val intentProcessors by lazyMonitored {
        IntentProcessors(
            context,
            core.store,
            useCases.sessionUseCases,
            useCases.tabsUseCases,
            useCases.customTabsUseCases,
            useCases.searchUseCases,
            core.relationChecker,
            core.customTabsStore,
            migrationStore,
            core.webAppManifestStorage
        )
    }

    val addonCollectionProvider by lazyMonitored {
        // Check if we have a customized (overridden) AMO collection (only supported in Nightly)
        if (Config.channel.isNightlyOrDebug && context.settings().amoCollectionOverrideConfigured()) {
            AddonCollectionProvider(
                context,
                core.client,
                collectionUser = context.settings().overrideAmoUser,
                collectionName = context.settings().overrideAmoCollection
            )
        }
        // Use build config otherwise
        else if (!BuildConfig.AMO_COLLECTION_USER.isNullOrEmpty() &&
            !BuildConfig.AMO_COLLECTION_NAME.isNullOrEmpty()
        ) {
            AddonCollectionProvider(
                context,
                core.client,
                serverURL = BuildConfig.AMO_SERVER_URL,
                collectionUser = BuildConfig.AMO_COLLECTION_USER,
                collectionName = BuildConfig.AMO_COLLECTION_NAME,
                maxCacheAgeInMinutes = AMO_COLLECTION_MAX_CACHE_AGE
            )
        }
        // Fall back to defaults
        else {
            AddonCollectionProvider(context, core.client, maxCacheAgeInMinutes = AMO_COLLECTION_MAX_CACHE_AGE)
        }
    }

    val appStartupTelemetry by lazyMonitored { AppStartupTelemetry(analytics.metrics) }

    @Suppress("MagicNumber")
    val addonUpdater by lazyMonitored {
        DefaultAddonUpdater(context, AddonUpdater.Frequency(12, TimeUnit.HOURS))
    }

    @Suppress("MagicNumber")
    val supportedAddonsChecker by lazyMonitored {
        DefaultSupportedAddonsChecker(context, SupportedAddonsChecker.Frequency(12, TimeUnit.HOURS),
            onNotificationClickIntent = Intent(context, HomeActivity::class.java).apply {
                action = Intent.ACTION_VIEW
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                data = "${BuildConfig.DEEP_LINK_SCHEME}://settings_addon_manager".toUri()
            }
        )
    }

    val addonManager by lazyMonitored {
        AddonManager(core.store, core.engine, addonCollectionProvider, addonUpdater)
    }

    val sitePermissionsStorage by lazyMonitored {
        SitePermissionsStorage(context, context.components.core.engine)
    }

    val analytics by lazyMonitored { Analytics(context) }
    val publicSuffixList by lazyMonitored { PublicSuffixList(context) }
    val clipboardHandler by lazyMonitored { ClipboardHandler(context) }
    val migrationStore by lazyMonitored { MigrationStore() }
    val performance by lazyMonitored { PerformanceComponent() }
    val push by lazyMonitored { Push(context, analytics.crashReporter) }
    val wifiConnectionMonitor by lazyMonitored { WifiConnectionMonitor(context as Application) }
    val strictMode by lazyMonitored { StrictModeManager(Config, this) }

    val settings by lazyMonitored { Settings(context) }

    val reviewPromptController by lazyMonitored {
        ReviewPromptController(
            context,
            FenixReviewSettings(settings)
        )
    }

    val autofillConfiguration by lazyMonitored {
        AutofillConfiguration(
            storage = core.passwordsStorage,
            publicSuffixList = publicSuffixList,
            unlockActivity = AutofillUnlockActivity::class.java,
            confirmActivity = AutofillConfiguration::class.java,
            applicationName = context.getString(R.string.app_name),
            httpClient = core.client
        )
    }
}

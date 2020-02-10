/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.addons.amo.AddonCollectionProvider
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.DefaultAddonUpdater
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.migration.state.MigrationStore
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.test.Mockable
import org.mozilla.fenix.utils.ClipboardHandler
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
            analytics.crashReporter,
            core.historyStorage,
            core.bookmarksStorage,
            core.syncablePasswordsStorage,
            core.getSecureAbove22Preferences()
        )
    }
    val services by lazy { Services(context, backgroundServices.accountManager) }
    val core by lazy { Core(context) }
    val search by lazy { Search(context) }
    val useCases by lazy {
        UseCases(
            context,
            core.sessionManager,
            core.store,
            core.engine.settings,
            search.searchEngineManager,
            core.webAppShortcutManager
        )
    }
    val intentProcessors by lazy {
        IntentProcessors(
            context,
            core.sessionManager,
            useCases.sessionUseCases,
            useCases.searchUseCases,
            core.client,
            core.customTabsStore,
            migrationStore,
            core.webAppManifestStorage
        )
    }

    val addonCollectionProvider by lazy {
        AddonCollectionProvider(context, core.client, maxCacheAgeInMinutes = DAY_IN_MINUTES)
    }

    val addonUpdater by lazy {
        DefaultAddonUpdater(context, AddonUpdater.Frequency(1, TimeUnit.DAYS))
    }

    val addonManager by lazy {
        AddonManager(core.store, core.engine, addonCollectionProvider, addonUpdater)
    }

    val browsingModeManager by lazy { DefaultBrowsingModeManager() }

    val tabsUseCases: TabsUseCases by lazy { TabsUseCases(core.sessionManager) }

    val analytics by lazy { Analytics(context) }
    val publicSuffixList by lazy { PublicSuffixList(context) }
    val clipboardHandler by lazy { ClipboardHandler(context) }
    val migrationStore by lazy { MigrationStore() }
}

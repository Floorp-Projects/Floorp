/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Context
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.locale.LocaleManager.getSystemDefault

/**
 * A single source for setting feature flags that are mostly based on build type.
 */
object FeatureFlags {
    /**
     * Pull-to-refresh allows you to pull the web content down far enough to have the page to
     * reload.
     */
    val pullToRefreshEnabled = Config.channel.isNightlyOrDebug

    /**
     * Enables the Addresses autofill feature.
     */
    val addressesFeature = Config.channel.isNightlyOrDebug

    /**
     * Enables WebAuthn support.
     */
    const val webAuthFeature = true

    /**
     * Enables the Home button in the browser toolbar to navigate back to the home screen.
     */
    const val showHomeButtonFeature = true

    /**
     * Enables the Start On Home feature in the settings page.
     */
    const val showStartOnHomeSettings = true

    /**
     * Enables the "recent" tabs feature in the home screen.
     */
    const val showRecentTabsFeature = true

    /**
     * Enables UI features based on history metadata.
     */
    const val historyMetadataUIFeature = true

    /**
     * Enables the recently saved bookmarks feature in the home screen.
     */
    const val recentBookmarksFeature = true

    /**
     * Identifies and separates the tabs list with a secondary section containing least used tabs.
     */
    const val inactiveTabs = true

    /**
     * Enables showing the home screen behind the search dialog
     */
    const val showHomeBehindSearch = true

    /**
     * Enables customizing the home screen
     */
    const val customizeHome = true

    /**
     * Identifies and separates the tabs list with a group containing search term tabs.
     */
    val tabGroupFeature = Config.channel.isNightlyOrDebug

    /**
     * Enables showing search groupings in the History.
     */
    const val showHistorySearchGroups = true

    /**
     * Show Pocket recommended stories on home.
     */
    fun isPocketRecommendationsFeatureEnabled(context: Context): Boolean {
        return "en-US" == LocaleManager.getCurrentLocale(context)
            ?.toLanguageTag() ?: getSystemDefault().toLanguageTag()
    }

    /**
     * Enables showing the homescreen onboarding card.
     */
    const val showHomeOnboarding = false
}

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
    val showHomeButtonFeature = Config.channel.isNightlyOrDebug

    /**
     * Enables the Start On Home feature in the settings page.
     */
    val showStartOnHomeSettings = Config.channel.isNightlyOrDebug

    /**
     * Enables the "recent" tabs feature in the home screen.
     */
    val showRecentTabsFeature = Config.channel.isNightlyOrDebug

    /**
     * Enables UI features based on history metadata.
     */
    val historyMetadataUIFeature = Config.channel.isNightlyOrDebug

    /**
     * Enables the recently saved bookmarks feature in the home screen.
     */
    val recentBookmarksFeature = Config.channel.isNightlyOrDebug

    /**
     * Identifies and separates the tabs list with a secondary section containing least used tabs.
     */
    val inactiveTabs = Config.channel.isNightlyOrDebug

    /**
     * Enables showing the home screen behind the search dialog
     */
    val showHomeBehindSearch = Config.channel.isNightlyOrDebug

    /**
     * Enables customizing the home screen
     */
    val customizeHome = Config.channel.isNightlyOrDebug

    /**
     * Identifies and separates the tabs list with a group containing search term tabs.
     */
    val tabGroupFeature = Config.channel.isNightlyOrDebug

    /**
     * Enables showing search groupings in the History.
     */
    val showHistorySearchGroups = Config.channel.isNightlyOrDebug

    /**
     * Show Pocket recommended stories on home.
     */
    fun isPocketRecommendationsFeatureEnabled(context: Context): Boolean {
        return Config.channel.isNightlyOrDebug &&
            "en-US" == LocaleManager.getCurrentLocale(context)?.toLanguageTag() ?: getSystemDefault().toLanguageTag()
    }

    /**
     * Enables showing the homescreen onboarding card.
     */
    val showHomeOnboarding = Config.channel.isDebug
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.selectedNormalTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.tabs.ext.hasMediaPlayed
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.tabstray.ext.isNormalTabInactive
import org.mozilla.fenix.utils.Settings
import java.util.concurrent.TimeUnit

/**
 * The time until which a tab is considered in-active (in days).
 */
const val DEFAULT_ACTIVE_DAYS = 14L

/**
 * The maximum time from when a tab was created or accessed until it is considered "inactive".
 */
val maxActiveTime = TimeUnit.DAYS.toMillis(DEFAULT_ACTIVE_DAYS)

/**
 * Get the last opened normal tab or last tab with in progress media.
 *
 * @return A list of the last opened tab or an empty list.
 */
fun BrowserState.asRecentTabs(): List<RecentTab> {
    return lastOpenedNormalTab?.let {
        mutableListOf(RecentTab.Tab(it))
    } ?: mutableListOf()
}

/**
 *  Get the selected normal tab or the last accessed normal tab
 *  if there is no selected tab or the selected tab is a private one.
 */
val BrowserState.lastOpenedNormalTab: TabSessionState?
    get() = selectedNormalTab ?: normalTabs.maxByOrNull { it.lastAccess }

/**
 *  Get the second-to-last accessed normal tab.
 */
val BrowserState.secondToLastOpenedNormalTab: TabSessionState?
    get() = when {
        normalTabs.size <= 1 -> null
        else -> normalTabs.sortedByDescending { it.lastAccess }[1]
    }

/**
 * Get the last tab with in progress media.
 */
val BrowserState.inProgressMediaTab: TabSessionState?
    get() = normalTabs
        .filter { it.hasMediaPlayed() }
        .maxByOrNull { it.lastMediaAccessState.lastMediaAccess }

/**
 * List of all inactive tabs based on [maxActiveTime].
 * The user may have disabled the feature so for user interactions consider using the [actualInactiveTabs] method
 * or an in place check of the feature status.
 */
val BrowserState.potentialInactiveTabs: List<TabSessionState>
    get() = normalTabs.filter { it.isNormalTabInactive(maxActiveTime) }

/**
 * List of all inactive tabs based on [maxActiveTime].
 * The result will be always be empty if the user disabled the feature.
 */
fun BrowserState.actualInactiveTabs(settings: Settings): List<TabSessionState> {
    return if (settings.inactiveTabsAreEnabled) {
        potentialInactiveTabs
    } else {
        emptyList()
    }
}

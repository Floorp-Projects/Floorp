/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.selectedNormalTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.tabs.ext.hasMediaPlayed
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.tabstray.SEARCH_TERM_TAB_GROUPS
import org.mozilla.fenix.tabstray.SEARCH_TERM_TAB_GROUPS_MIN_SIZE
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
 * Get the last opened normal tab, last tab with in progress media and last search term group, if available.
 *
 * @return A list of the last opened tab not part of the last active search group and
 * the last active search group if these are available or an empty list.
 */
fun BrowserState.asRecentTabs(): List<RecentTab> {
    return mutableListOf<RecentTab>().apply {
        val mostRecentTabsGroup = lastSearchGroup
        val mostRecentTabNotInGroup = if (mostRecentTabsGroup == null) {
            lastOpenedNormalTab
        } else {
            listOf(selectedNormalTab)
                .plus(normalTabs.sortedByDescending { it.lastAccess })
                .filterNot { lastTabGroup?.tabIds?.contains(it?.id) ?: false }
                .firstOrNull()
        }

        mostRecentTabNotInGroup?.let { add(RecentTab.Tab(it)) }

        mostRecentTabsGroup?.let { add(it) }
    }
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
 * Get the most recently accessed [TabGroup].
 * Result will be `null` if the currently open normal tabs are not part of a search group.
 */
val BrowserState.lastTabGroup: TabGroup?
    get() = tabPartitions[SEARCH_TERM_TAB_GROUPS]?.tabGroups
        ?.lastOrNull { it.tabIds.size >= SEARCH_TERM_TAB_GROUPS_MIN_SIZE }

/**
 * Get the most recent search term group.
 */
val BrowserState.lastSearchGroup: RecentTab.SearchGroup?
    get() {
        val tabGroup = lastTabGroup ?: return null
        val firstTabId = tabGroup.tabIds.firstOrNull() ?: return null
        val firstTab = findTab(firstTabId) ?: return null

        return RecentTab.SearchGroup(
            tabGroup.id,
            firstTabId,
            firstTab.content.url,
            firstTab.content.thumbnail,
            tabGroup.tabIds.size
        )
    }

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

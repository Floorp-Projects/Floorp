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
import org.mozilla.fenix.tabstray.browser.TabGroup
import org.mozilla.fenix.tabstray.browser.maxActiveTime
import org.mozilla.fenix.tabstray.ext.isNormalTabActiveWithSearchTerm
import kotlin.math.max

/**
 * Get the last opened normal tab, last tab with in progress media and last search term group, if available.
 *
 * @return A list of the last opened tab, last tab with in progress media and last search term group
 * if distinct and available or an empty list.
 */
fun BrowserState.asRecentTabs(): List<RecentTab> {
    return mutableListOf<RecentTab>().apply {
        val lastOpenedNormalTab = lastOpenedNormalTab

        lastOpenedNormalTab?.let { add(RecentTab.Tab(it)) }

        lastSearchGroup?.let { add(it) }
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
 * Get the most recent search term group.
 */
val BrowserState.lastSearchGroup: RecentTab.SearchGroup?
    get() {
        val tabGroup = normalTabs.toSearchGroup().first.lastOrNull() ?: return null
        val firstTab = tabGroup.tabs.firstOrNull() ?: return null

        return RecentTab.SearchGroup(
            tabGroup.searchTerm,
            firstTab.id,
            firstTab.content.url,
            firstTab.content.thumbnail,
            tabGroup.tabs.count()
        )
    }

/**
 * Returns a pair containing a list of search term groups sorted by last access time, and "remainder" tabs that have
 * search terms but should not be in groups (because the group is of size one).
 */
fun List<TabSessionState>.toSearchGroup(): Pair<List<TabGroup>, List<TabSessionState>> {
    val data = filter {
        it.isNormalTabActiveWithSearchTerm(maxActiveTime)
    }.groupBy {
        when {
            it.content.searchTerms.isNotBlank() -> it.content.searchTerms
            else -> it.historyMetadata?.searchTerm ?: ""
        }.lowercase()
    }

    val groupings = data.map { mapEntry ->
        val searchTerm = mapEntry.key.replaceFirstChar(Char::uppercase)
        val groupTabs = mapEntry.value
        val groupMax = groupTabs.fold(0L) { acc, tab ->
            max(tab.lastAccess, acc)
        }

        TabGroup(
            searchTerm = searchTerm,
            tabs = groupTabs,
            lastAccess = groupMax
        )
    }

    val groups = groupings.filter { it.tabs.size > 1 }.sortedBy { it.lastAccess }
    val remainderTabs = (groupings - groups).flatMap { it.tabs }

    return groups to remainderTabs
}

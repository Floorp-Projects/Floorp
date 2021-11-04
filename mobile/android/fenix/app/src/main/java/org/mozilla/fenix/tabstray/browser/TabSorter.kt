/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.recyclerview.widget.ConcatAdapter
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.feature.tabs.tabstray.TabsFeature
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.maxActiveTime
import org.mozilla.fenix.ext.toSearchGroup
import org.mozilla.fenix.tabstray.ext.browserAdapter
import org.mozilla.fenix.tabstray.ext.hasSearchTerm
import org.mozilla.fenix.tabstray.ext.inactiveTabsAdapter
import org.mozilla.fenix.tabstray.ext.isActive
import org.mozilla.fenix.tabstray.ext.isNormalTabActiveWithSearchTerm
import org.mozilla.fenix.tabstray.ext.tabGroupAdapter
import org.mozilla.fenix.tabstray.ext.titleHeaderAdapter
import org.mozilla.fenix.utils.Settings

/**
 * An intermediary layer to consume tabs from [TabsFeature] for sorting into the various adapters.
 */
class TabSorter(
    private val settings: Settings,
    private val metrics: MetricController,
    private val concatAdapter: ConcatAdapter
) : TabsTray {
    private var shouldReportMetrics: Boolean = true
    private val groupsSet = mutableSetOf<String>()

    override fun updateTabs(tabs: List<TabSessionState>, selectedTabId: String?) {
        val inactiveTabs = tabs.getInactiveTabs(settings)
        val searchTermTabs = tabs.getSearchGroupTabs(settings)
        val normalTabs = tabs - inactiveTabs - searchTermTabs

        // Inactive tabs
        concatAdapter.inactiveTabsAdapter.updateTabs(inactiveTabs, selectedTabId)

        // Tab groups
        // We don't need to provide a selectedId, because the [TabGroupAdapter] has that built-in with support from
        //  NormalBrowserPageViewHolder.scrollToTab.
        val (groups, remainderTabs) = searchTermTabs.toSearchGroup(groupsSet)

        groupsSet.clear()
        groupsSet.addAll(groups.map { it.searchTerm })
        concatAdapter.tabGroupAdapter.submitList(groups)

        // Normal tabs.
        val totalNormalTabs = (normalTabs + remainderTabs)
        concatAdapter.browserAdapter.updateTabs(totalNormalTabs, selectedTabId)

        // Normal tab title header.
        concatAdapter.titleHeaderAdapter
            .handleListChanges(totalNormalTabs.isNotEmpty() && groups.isNotEmpty())

        if (shouldReportMetrics) {
            shouldReportMetrics = false

            if (settings.inactiveTabsAreEnabled) {
                metrics.track(Event.TabsTrayHasInactiveTabs(inactiveTabs.size))
            }

            if (groups.isNotEmpty()) {
                val averageTabsPerGroup = groups.map { it.tabs.size }.average()
                metrics.track(Event.AverageTabsPerSearchTermGroup(averageTabsPerGroup))
            }
            metrics.track(Event.SearchTermGroupCount(groups.size))
        }
    }
}

/**
 * Returns a list of inactive tabs based on our preferences.
 */
private fun List<TabSessionState>.getInactiveTabs(settings: Settings): List<TabSessionState> {
    val inactiveTabsEnabled = settings.inactiveTabsAreEnabled
    return if (inactiveTabsEnabled) {
        filter { !it.isActive(maxActiveTime) }
    } else {
        emptyList()
    }
}

/**
 * Returns a list of search term tabs based on our preferences.
 */
private fun List<TabSessionState>.getSearchGroupTabs(settings: Settings): List<TabSessionState> {
    val inactiveTabsEnabled = settings.inactiveTabsAreEnabled
    val tabGroupsEnabled = settings.searchTermTabGroupsAreEnabled
    return when {
        tabGroupsEnabled && inactiveTabsEnabled ->
            filter { it.isNormalTabActiveWithSearchTerm(maxActiveTime) }

        tabGroupsEnabled ->
            filter { it.hasSearchTerm() }

        else -> emptyList()
    }
}

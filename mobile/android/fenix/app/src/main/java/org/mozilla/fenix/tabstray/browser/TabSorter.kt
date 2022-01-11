/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.feature.tabs.tabstray.TabsFeature
import org.mozilla.fenix.ext.maxActiveTime
import org.mozilla.fenix.tabstray.SEARCH_TERM_TAB_GROUPS_MIN_SIZE
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.ext.isActive
import org.mozilla.fenix.utils.Settings

/**
 * An intermediary layer to consume tabs from [TabsFeature] for sorting into the various adapters.
 */
class TabSorter(
    private val settings: Settings,
    private val tabsTrayStore: TabsTrayStore? = null
) : TabsTray {
    private val groupsSet = mutableSetOf<String>()

    override fun updateTabs(tabs: List<TabSessionState>, tabPartition: TabPartition?, selectedTabId: String?) {
        val privateTabs = tabs.filter { it.content.private }
        val allNormalTabs = tabs - privateTabs
        val inactiveTabs = allNormalTabs.getInactiveTabs(settings)
        val tabGroups = tabPartition?.getTabGroups(settings, groupsSet) ?: emptyList()
        val tabGroupTabIds = tabGroups.flatMap { it.tabIds }
        val normalTabs = (allNormalTabs - inactiveTabs).filterNot { tabGroupTabIds.contains(it.id) }
        val minTabPartition = tabPartition?.let { TabPartition(tabPartition.id, tabGroups) }

        // Private tabs
        tabsTrayStore?.dispatch(TabsTrayAction.UpdatePrivateTabs(privateTabs))

        // Inactive tabs
        tabsTrayStore?.dispatch(TabsTrayAction.UpdateInactiveTabs(inactiveTabs))

        // Normal tabs
        tabsTrayStore?.dispatch(TabsTrayAction.UpdateNormalTabs(normalTabs))

        // Search term tabs
        tabsTrayStore?.dispatch(TabsTrayAction.UpdateTabPartitions(minTabPartition))

        groupsSet.clear()
        groupsSet.addAll(tabGroups.map { it.id })
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
 * Returns a list of tab groups based on our preferences.
 */
private fun TabPartition.getTabGroups(settings: Settings, groupsSet: Set<String>): List<TabGroup> {
    return if (settings.searchTermTabGroupsAreEnabled) {
        tabGroups.filter {
            it.tabIds.size >= SEARCH_TERM_TAB_GROUPS_MIN_SIZE || groupsSet.contains(it.id)
        }
    } else {
        emptyList()
    }
}

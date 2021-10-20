/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.recyclerview.widget.ConcatAdapter
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.tabstray.TabsFeature
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.tabstray.ext.browserAdapter
import org.mozilla.fenix.tabstray.ext.inactiveTabsAdapter
import org.mozilla.fenix.tabstray.ext.tabGroupAdapter
import org.mozilla.fenix.utils.Settings
import kotlin.math.max

/**
 * An intermediary layer to consume tabs from [TabsFeature] for sorting into the various adapters.
 */
class TabSorter(
    private val settings: Settings,
    private val metrics: MetricController,
    private val concatAdapter: ConcatAdapter,
    private val store: BrowserStore
) : TabsTray, Observable<TabsTray.Observer> by ObserverRegistry() {
    private val groupsSet = mutableSetOf<String>()

    override fun updateTabs(tabs: Tabs) {
        val inactiveTabs = tabs.list.getInactiveTabs(settings)
        val searchTermTabs = tabs.list.getSearchGroupTabs(settings)
        val normalTabs = tabs.list - inactiveTabs - searchTermTabs
        val selectedTabId = store.state.selectedTabId

        // Inactive tabs
        val selectedInactiveIndex = inactiveTabs.findSelectedIndex(selectedTabId)
        concatAdapter.inactiveTabsAdapter.updateTabs((Tabs(inactiveTabs, selectedInactiveIndex)))
        if (settings.inactiveTabsAreEnabled) {
            metrics.track(Event.TabsTrayHasInactiveTabs(inactiveTabs.size))
        }

        // Tab groups
        // We don't need to provide a selectedId, because the [TabGroupAdapter] has that built-in with support from
        //  NormalBrowserPageViewHolder.scrollToTab.
        val (groups, remainderTabs) = searchTermTabs.toSearchGroups(groupsSet)

        groupsSet.clear()
        groupsSet.addAll(groups.map { it.title })

        concatAdapter.tabGroupAdapter.submitList(groups)

        // Normal tabs.
        val totalNormalTabs = (normalTabs + remainderTabs)
        val selectedTabIndex = totalNormalTabs.findSelectedIndex(selectedTabId)
        concatAdapter.browserAdapter.updateTabs(Tabs(totalNormalTabs, selectedTabIndex))
    }

    override fun isTabSelected(tabs: Tabs, position: Int): Boolean = false
}

private fun List<Tab>.findSelectedIndex(tabId: String?): Int {
    val id = tabId ?: return -1
    return indexOfFirst { it.id == id }
}

/**
 * Returns a list of inactive tabs based on our preferences.
 */
private fun List<Tab>.getInactiveTabs(settings: Settings): List<Tab> {
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
private fun List<Tab>.getSearchGroupTabs(settings: Settings): List<Tab> {
    val inactiveTabsEnabled = settings.inactiveTabsAreEnabled
    val tabGroupsEnabled = settings.searchTermTabGroupsAreEnabled
    return when {
        tabGroupsEnabled && inactiveTabsEnabled ->
            filter { it.searchTerm.isNotBlank() && it.isActive(maxActiveTime) }

        tabGroupsEnabled ->
            filter { it.searchTerm.isNotBlank() }

        else -> emptyList()
    }
}

/**
 * Returns true if a tab has not been selected since [maxActiveTime].
 *
 * N.B: This is duplicated from [TabSessionState.isActive(Long)] to work for [Tab].
 *
 * See also: https://github.com/mozilla-mobile/android-components/issues/11012
 */
private fun Tab.isActive(maxActiveTime: Long): Boolean {
    val lastActiveTime = maxOf(lastAccess, createdAt)
    val now = System.currentTimeMillis()
    return (now - lastActiveTime <= maxActiveTime)
}

/**
 * Creates a list of grouped search term tabs sorted by last access time and a list of tabs
 * that have search terms but would only create groups with a single tab.
 *
 * N.B: This is duplicated from [List<TabSessionState>.toSearchGroup()] to work for [Tab].
 *
 * See also: https://github.com/mozilla-mobile/android-components/issues/11012
 */
private fun List<Tab>.toSearchGroups(groupSet: Set<String>): Pair<List<TabGroupAdapter.Group>, List<Tab>> {
    val data = groupBy { it.searchTerm.lowercase() }

    val groupings = data.map { mapEntry ->
        // Uppercase since we use it for the title.
        val searchTerm = mapEntry.key.replaceFirstChar(Char::uppercase)
        val groupTabs = mapEntry.value

        // Calculate when the group was last used.
        val groupMax = groupTabs.fold(0L) { acc, tab ->
            max(tab.lastAccess, acc)
        }

        TabGroupAdapter.Group(
            title = searchTerm,
            tabs = groupTabs,
            lastAccess = groupMax
        )
    }

    val groups = groupings.filter { it.tabs.size > 1 || groupSet.contains(it.title) }.sortedBy { it.lastAccess }
    val remainderTabs = (groupings - groups).flatMap { it.tabs }

    return groups to remainderTabs
}

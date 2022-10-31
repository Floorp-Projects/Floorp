/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.recover.toTabSessionStates
import kotlin.math.max

internal object TabListReducer {
    /**
     * [TabListAction] Reducer function for modifying the list of [TabSessionState] objects in [BrowserState.tabs].
     */
    @Suppress("LongMethod")
    fun reduce(state: BrowserState, action: TabListAction): BrowserState {
        return when (action) {
            is TabListAction.AddTabAction -> {
                // Verify that tab doesn't already exist
                requireUniqueTab(state, action.tab)

                val updatedTabList = if (action.tab.parentId != null) {
                    val parentIndex = state.tabs.indexOfFirst { it.id == action.tab.parentId }
                    if (parentIndex == -1) {
                        throw IllegalArgumentException("The parent does not exist")
                    }

                    // Add the child tab next to its parent
                    val childIndex = parentIndex + 1
                    state.tabs.subList(0, childIndex) + action.tab + state.tabs.subList(childIndex, state.tabs.size)
                } else {
                    state.tabs + action.tab
                }

                state.copy(
                    tabs = updatedTabList,
                    selectedTabId = if (action.select || state.selectedTabId == null) {
                        action.tab.id
                    } else {
                        state.selectedTabId
                    },
                )
            }

            is TabListAction.AddMultipleTabsAction -> {
                action.tabs.forEach { requireUniqueTab(state, it) }

                action.tabs.find { tab -> tab.parentId != null }?.let {
                    throw IllegalArgumentException("Adding multiple tabs with a parent id is not supported")
                }

                state.copy(
                    tabs = state.tabs + action.tabs,
                    selectedTabId = if (state.selectedTabId == null) {
                        action.tabs.find { tab -> !tab.content.private }?.id
                    } else {
                        state.selectedTabId
                    },
                )
            }

            is TabListAction.MoveTabsAction -> {
                val tabTarget = state.findTab(action.targetTabId)
                if (tabTarget == null) {
                    state
                } else {
                    val targetPosition = state.tabs.indexOf(tabTarget) + (if (action.placeAfter) 1 else 0)
                    val positionOffset = state.tabs.filterIndexed { index, tab ->
                        (index < targetPosition && tab.id in action.tabIds)
                    }.count()
                    val finalPos = targetPosition - positionOffset
                    val (movedTabs, unmovedTabs) = state.tabs.partition { it.id in action.tabIds }
                    val updatedTabList = unmovedTabs.subList(0, finalPos) +
                        movedTabs +
                        unmovedTabs.subList(finalPos, unmovedTabs.size)

                    state.copy(tabs = updatedTabList)
                }
            }

            is TabListAction.SelectTabAction -> {
                state.copy(selectedTabId = action.tabId)
            }

            is TabListAction.RemoveTabAction -> {
                val tabToRemove = state.findTab(action.tabId)

                if (tabToRemove == null) {
                    state
                } else {
                    // Remove tab and update child tabs in case their parent was removed
                    val updatedTabList = (state.tabs - tabToRemove).map {
                        if (it.parentId == tabToRemove.id) it.copy(parentId = tabToRemove.parentId) else it
                    }

                    val updatedSelection = if (action.selectParentIfExists && tabToRemove.parentId != null) {
                        // The parent tab should be selected if one exists
                        tabToRemove.parentId
                    } else if (state.selectedTabId == tabToRemove.id) {
                        // The selected tab was removed and we need to find a new one
                        val previousIndex = state.tabs.indexOf(tabToRemove)
                        findNewSelectedTabId(updatedTabList, tabToRemove.content.private, previousIndex)
                    } else {
                        // The selected tab is not affected and can stay the same
                        state.selectedTabId
                    }

                    state.copy(
                        tabs = updatedTabList,
                        selectedTabId = updatedSelection,
                        tabPartitions = state.tabPartitions.removeTabs(listOf(action.tabId)),
                    )
                }
            }

            is TabListAction.RemoveTabsAction -> {
                val tabsToRemove = action.tabIds.mapNotNull { state.findTab(it) }

                if (tabsToRemove.isNullOrEmpty()) {
                    state
                } else {
                    // Remove tabs and update child tabs' parentId if their parent is in removed list
                    val updatedTabList = (state.tabs - tabsToRemove).map { tabState ->
                        tabsToRemove.firstOrNull { removedTab -> tabState.parentId == removedTab.id }
                            ?.let { tabState.copy(parentId = findNewParentId(it, tabsToRemove)) }
                            ?: tabState
                    }

                    val updatedSelection =
                        if (action.tabIds.contains(state.selectedTabId)) {
                            val removedSelectedTab =
                                tabsToRemove.first { it.id == state.selectedTabId }
                            // The selected tab was removed and we need to find a new one
                            val previousIndex = state.tabs.indexOf(removedSelectedTab)
                            findNewSelectedTabId(
                                updatedTabList,
                                removedSelectedTab.content.private,
                                previousIndex,
                            )
                        } else {
                            // The selected tab is not affected and can stay the same
                            state.selectedTabId
                        }

                    state.copy(
                        tabs = updatedTabList,
                        selectedTabId = updatedSelection,
                        tabPartitions = state.tabPartitions.removeTabs(action.tabIds),
                    )
                }
            }

            is TabListAction.RestoreAction -> {
                // Verify that none of the tabs to restore already exist
                val restoredTabs = action.tabs.toTabSessionStates()
                restoredTabs.forEach { requireUniqueTab(state, it) }

                // Using the enum, action.restoreLocation, we are adding the restored tabs at
                // either the beginning of the tab list, the end of the tab list, or at a
                // specified index (RecoverableTab.index). If the index for some reason is -1,
                // the tab will be restored to the end of the tab list. Upon restoration, the
                // index will be reset to -1 when added to the combined list.
                val combinedTabList: List<TabSessionState> = when (action.restoreLocation) {
                    TabListAction.RestoreAction.RestoreLocation.BEGINNING -> restoredTabs + state.tabs
                    TabListAction.RestoreAction.RestoreLocation.END -> state.tabs + restoredTabs
                    TabListAction.RestoreAction.RestoreLocation.AT_INDEX -> mutableListOf<TabSessionState>().apply {
                        addAll(state.tabs)
                        restoredTabs.forEachIndexed { index, restoredTab ->
                            val tabIndex = action.tabs[index].state.index
                            val restoreIndex =
                                if (tabIndex > size || tabIndex < 0) {
                                    size
                                } else {
                                    tabIndex
                                }
                            add(restoreIndex, restoredTab)
                        }
                    }
                }

                state.copy(
                    tabs = combinedTabList,
                    selectedTabId = if (action.selectedTabId != null && state.selectedTabId == null) {
                        // We only want to update the selected tab if none has been already selected. Otherwise we may
                        // switch to a restored tab even though the user is already looking at an existing tab (e.g.
                        // a tab that came from an intent).
                        action.selectedTabId
                    } else {
                        state.selectedTabId
                    },
                )
            }

            is TabListAction.RemoveAllTabsAction -> {
                state.copy(
                    tabs = emptyList(),
                    selectedTabId = null,
                    tabPartitions = state.tabPartitions.removeAllTabs(),
                )
            }

            is TabListAction.RemoveAllPrivateTabsAction -> {
                val selectionAffected = state.selectedTab?.content?.private == true
                val partition = state.tabs.partition { it.content.private }
                val normalTabs = partition.second
                state.copy(
                    tabs = normalTabs,
                    selectedTabId = if (selectionAffected) {
                        // If the selection is affected, select the last normal tab, if available.
                        normalTabs.lastOrNull()?.id
                    } else {
                        state.selectedTabId
                    },
                    tabPartitions = state.tabPartitions.removeTabs(partition.first.map { it.id }),
                )
            }

            is TabListAction.RemoveAllNormalTabsAction -> {
                val selectionAffected = state.selectedTab?.content?.private == false
                val partition = state.tabs.partition { it.content.private }
                val privateTabs = partition.first
                state.copy(
                    tabs = privateTabs,
                    selectedTabId = if (selectionAffected) {
                        // If the selection is affected, we'll set it to null as there's no
                        // normal tab left and NO private tab should get selected instead.
                        null
                    } else {
                        state.selectedTabId
                    },
                    tabPartitions = state.tabPartitions.removeTabs(partition.second.map { it.id }),
                )
            }
        }
    }
}

/**
 * Looks for an appropriate new parentId for a tab by checking if the parent is being removed,
 * and if so, will recursively check the parent's parent and so on.
 */
private fun findNewParentId(
    tabToFindNewParent: TabSessionState,
    tabsToBeRemoved: List<TabSessionState>,
): String? {
    return if (tabsToBeRemoved.map { it.id }.contains(tabToFindNewParent.parentId)) {
        // The parent tab is being removed, let's check the parent's parent
        findNewParentId(
            tabsToBeRemoved.first { tabToFindNewParent.parentId == it.id },
            tabsToBeRemoved,
        )
    } else {
        tabToFindNewParent.parentId
    }
}

/**
 * Find a new selected tab and return its id after the tab at [previousIndex] was removed.
 */
private fun findNewSelectedTabId(
    tabs: List<TabSessionState>,
    isPrivate: Boolean,
    previousIndex: Int,
): String? {
    if (tabs.isEmpty()) {
        // There's no tab left to select.
        return null
    }

    val predicate: (TabSessionState) -> Boolean = { tab -> tab.content.private == isPrivate }

    // If the previous index is still a valid index and if this is a private/normal tab we are looking for then
    // let's use the tab at the same index.
    if (previousIndex <= tabs.lastIndex && predicate(tabs[previousIndex])) {
        return tabs[previousIndex].id
    }

    // Find a tab that matches the predicate and is nearby the tab that was selected previously
    val nearbyTab = findNearbyTab(tabs, previousIndex, predicate)

    return when {
        // We found a nearby tab, let's select it.
        nearbyTab != null -> nearbyTab.id

        // If there's no private tab to select anymore then just select the last regular tab
        isPrivate -> tabs.last().id

        // Removing the last normal tab should NOT cause a private tab to be selected
        else -> null
    }
}

/**
 * Find a tab that is near the provided [index] and matches the [predicate].
 */
private fun findNearbyTab(
    tabs: List<TabSessionState>,
    index: Int,
    predicate: (TabSessionState) -> Boolean,
): TabSessionState? {
    val maxSteps = max(tabs.lastIndex - index, index)
    if (maxSteps < 0) {
        return null
    }

    // Try tabs oscillating near the index.
    for (steps in 1..maxSteps) {
        listOf(index - steps, index + steps).forEach { current ->
            if (current in 0..tabs.lastIndex &&
                predicate(tabs[current])
            ) {
                return tabs[current]
            }
        }
    }

    return null
}

/**
 * Checks that the provided tab doesn't already exist and throws an
 * [IllegalArgumentException] otherwise.
 *
 * @param state the current [BrowserState] (including all existing tabs).
 * @param tab the [TabSessionState] to check.
 */
private fun requireUniqueTab(state: BrowserState, tab: TabSessionState) {
    require(state.tabs.find { it.id == tab.id } == null) {
        "Tab with same ID already exists"
    }
}

/**
 * Removes references to the provided tabs from all [TabPartition]s.
 */
private fun Map<String, TabPartition>.removeTabs(removedTabIds: List<String>) =
    mapValues {
        val partition = it.value
        partition.copy(
            tabGroups = partition.tabGroups.map { group ->
                group.copy(tabIds = group.tabIds.filterNot { tabId -> removedTabIds.contains(tabId) })
            },
        )
    }

/**
 * Removes references to the provided tabs from all [TabPartition]s.
 */
private fun Map<String, TabPartition>.removeAllTabs() =
    mapValues {
        val partition = it.value
        partition.copy(
            tabGroups = partition.tabGroups.map { group -> group.copy(tabIds = emptyList()) },
        )
    }

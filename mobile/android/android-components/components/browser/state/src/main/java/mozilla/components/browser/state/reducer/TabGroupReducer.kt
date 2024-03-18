/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.TabGroupAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.getGroupById

internal object TabGroupReducer {

    /**
     * [TabGroupAction] reducer function for modifying tab groups in [BrowserState.tabPartitions].
     */
    fun reduce(state: BrowserState, action: TabGroupAction): BrowserState {
        return when (action) {
            is TabGroupAction.AddTabGroupAction -> {
                action.group.tabIds.forEach { state.assertTabExists(it) }
                state.addTabGroup(action.partition, action.group)
            }

            is TabGroupAction.RemoveTabGroupAction -> {
                state.removeTabGroup(action.partition, action.group)
            }

            is TabGroupAction.AddTabAction -> {
                state.assertTabExists(action.tabId)

                if (!state.groupExists(action.partition, action.group)) {
                    state.addTabGroup(action.partition, TabGroup(action.group, tabIds = listOf(action.tabId)))
                } else {
                    state.updateTabGroup(action.partition, action.group) {
                        it.copy(tabIds = (it.tabIds + action.tabId).distinct())
                    }
                }
            }

            is TabGroupAction.AddTabsAction -> {
                action.tabIds.forEach { state.assertTabExists(it) }

                if (!state.groupExists(action.partition, action.group)) {
                    state.addTabGroup(action.partition, TabGroup(action.group, tabIds = action.tabIds.distinct()))
                } else {
                    state.updateTabGroup(action.partition, action.group) {
                        it.copy(tabIds = (it.tabIds + action.tabIds).distinct())
                    }
                }
            }

            is TabGroupAction.RemoveTabAction -> {
                state.updateTabGroup(action.partition, action.group) {
                    it.copy(tabIds = it.tabIds - action.tabId)
                }
            }

            is TabGroupAction.RemoveTabsAction -> {
                state.updateTabGroup(action.partition, action.group) {
                    it.copy(tabIds = it.tabIds - action.tabIds)
                }
            }
        }
    }
}

/**
 * Adds the provided tab group and creates the partition if needed.
 */
private fun BrowserState.addTabGroup(partitionId: String, group: TabGroup): BrowserState {
    val partition = tabPartitions[partitionId]
    val updatedPartition = if (partition != null) {
        if (partition.getGroupById(group.id) != null) {
            throw IllegalArgumentException("Tab group with same ID already exists")
        }
        partition.copy(tabGroups = partition.tabGroups + group)
    } else {
        TabPartition(partitionId, tabGroups = listOf(group))
    }
    return copy(tabPartitions = tabPartitions + (partitionId to updatedPartition))
}

/**
 * Removes a tab group from the provided partition.
 */
private fun BrowserState.removeTabGroup(partitionId: String, groupId: String): BrowserState {
    val partition = tabPartitions[partitionId]
    val group = partition?.getGroupById(groupId)
    return if (group != null) {
        val updatedPartition = partition.copy(tabGroups = partition.tabGroups - group)
        if (updatedPartition.tabGroups.isEmpty()) {
            copy(tabPartitions = tabPartitions - partitionId)
        } else {
            copy(tabPartitions = tabPartitions + (partitionId to updatedPartition))
        }
    } else {
        this
    }
}

/**
 * Checks if a tab group exists in the provided partition.
 */
private fun BrowserState.groupExists(partitionId: String, groupId: String): Boolean {
    return tabPartitions[partitionId]?.getGroupById(groupId) != null
}

/**
 * Checks that the provided tab exists and throws an
 * [IllegalArgumentException] otherwise.
 *
 * @param tabId the id of the [TabSessionState] to check.
 */
private fun BrowserState.assertTabExists(tabId: String) {
    require(tabs.find { it.id == tabId } != null) {
        "Tab does not exist"
    }
}

/**
 * Utility function to update a [TabGroup] within a [TabPartition] in [BrowserState].
 */
private fun BrowserState.updateTabGroup(
    partitionId: String,
    groupId: String,
    update: (TabGroup) -> TabGroup,
): BrowserState {
    return updateTabPartition(partitionId) { partition ->
        partition.updateTabGroup(groupId, update)
    }
}

/**
 * Updates the specified tab partition by invoking [update].
 */
private inline fun BrowserState.updateTabPartition(
    partitionId: String,
    crossinline update: (TabPartition) -> TabPartition,
): BrowserState {
    val partition = tabPartitions[partitionId] ?: return this
    return copy(tabPartitions = tabPartitions + (partitionId to update(partition)))
}

/**
 * Updates the specified tab group within this partition by invoking [update].
 */
private inline fun TabPartition.updateTabGroup(
    groupId: String,
    crossinline update: (TabGroup) -> TabGroup,
): TabPartition {
    return tabGroups.update(groupId, update)?.let {
        copy(tabGroups = it)
    } ?: this
}

/**
 * Updates the provided tab group by invoking [update].
 */
private inline fun List<TabGroup>.update(
    groupId: String,
    crossinline update: (TabGroup) -> TabGroup,
): List<TabGroup>? {
    val groupIndex = indexOfFirst { it.id == groupId }
    if (groupIndex == -1) return null

    return subList(0, groupIndex) + update(get(groupIndex)) + subList(groupIndex + 1, size)
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import java.util.UUID

/**
 * Value type representing a tab partition. Partitions can overlap i.e., a tab
 * can be in multiple partitions at the same time.
 *
 * @property id The ID of a tab partition. This should uniquely identify
 * the feature responsible for managing those groups.
 * @property tabGroups The groups of tabs in this partition. A partition can
 * have one or more groups, depending on use case. Empty partitions will be
 * removed by the system.
 */
data class TabPartition(
    val id: String,
    val tabGroups: List<TabGroup> = emptyList(),
)

/**
 * Value type representing a tab group.
 *
 * @property id The unique ID of this tab group.
 * @property name The name of this tab group.
 * @property tabIds The IDs of all tabs in this group.
 */
data class TabGroup(
    val id: String = UUID.randomUUID().toString(),
    val name: String = "",
    val tabIds: List<String> = emptyList(),
)

/**
 * Returns the first tab group matching the provided [name], or null if no match
 * was found. Note that we allow multiple groups with the same name in a
 * partition but disambiguation needs to be handled on a feature level.
 */
fun TabPartition.getGroupByName(name: String) = this.tabGroups.firstOrNull {
    it.name.equals(name, ignoreCase = true)
}

/**
 * Returns the tab group matching the provided [id], or null if not match was found.
 */
fun TabPartition.getGroupById(id: String) = this.tabGroups.firstOrNull {
    it.id == id
}

/**
 * Check if a [TabPartition] has no tabs
 *
 * @return true if the [TabPartition] has no tabs, false otherwise.
 */
fun TabPartition?.isEmpty(): Boolean {
    return this?.tabGroups?.filter { tabGroup -> tabGroup.tabIds.isNotEmpty() }
        .isNullOrEmpty()
}

/**
 * Check if a [TabPartition] has tabs
 *
 * @return true if the [TabPartition] has tabs, false otherwise.
 */
fun TabPartition?.isNotEmpty(): Boolean {
    return isEmpty().not()
}

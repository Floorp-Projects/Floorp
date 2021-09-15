/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata

import mozilla.components.concept.storage.HistoryMetadata

/**
 * A history metadata group.
 *
 * @property title The title of the group.
 * @property historyMetadata A list of [HistoryMetadata] records that matches the title.
 * @property expanded Whether or not the group is expanded.
 */
data class HistoryMetadataGroup(
    val title: String,
    val historyMetadata: List<HistoryMetadata>,
    val expanded: Boolean = false
)

// The last updated time of the group is based on the most recently updated item in the group
fun HistoryMetadataGroup.lastUpdated(): Long = historyMetadata.maxOf { it.updatedAt }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.db

import androidx.room.Entity
import androidx.room.PrimaryKey

/**
 * A Pocket recommended story that is to be mapped to SQLite table.
 */
@Entity(tableName = PocketRecommendationsDatabase.TABLE_NAME_STORIES)
internal data class PocketStoryEntity(
    @PrimaryKey
    val url: String,
    val title: String,
    val imageUrl: String,
    val publisher: String,
    val category: String,
    val timeToRead: Int,
    val timesShown: Long,
)

/**
 * A [PocketStoryEntity] only containing data about the [timesShown] property allowing for quick updates.
 */
internal data class PocketLocalStoryTimesShown(
    val url: String,
    val timesShown: Long,
)

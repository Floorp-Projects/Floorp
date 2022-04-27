/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.db

import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDatabase

/**
 * A sponsored Pocket story that is to be mapped to SQLite table.
 */
@Entity(tableName = PocketRecommendationsDatabase.TABLE_NAME_SPOCS)
internal data class SpocEntity(
    @PrimaryKey
    val url: String,
    val title: String,
    val imageUrl: String,
    val sponsor: String,
    val clickShim: String,
    val impressionShim: String,
)

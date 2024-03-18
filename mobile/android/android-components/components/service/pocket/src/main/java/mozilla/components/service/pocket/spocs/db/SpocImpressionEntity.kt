/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.db

import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.Index
import androidx.room.PrimaryKey
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDatabase

/**
 * One sponsored Pocket story impression.
 * Allows to easily create a relation between a particular spoc identified by it's [SpocEntity.id]
 * and any number of impressions.
 *
 * @property spocId [SpocEntity.id] that this serves as an impression of.
 * Used as a foreign key allowing to only add impressions for other persisted spocs and
 * automatically remove all impressions when the spoc they refer to is deleted.
 * @property impressionId Unique id of this entity. Primary key.
 * @property impressionDateInSeconds Epoch based timestamp expressed in seconds (from System.currentTimeMillis / 1000)
 * for when the spoc identified by [spocId] was shown to the user.
 */
@Entity(
    tableName = PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS,
    foreignKeys = [
        ForeignKey(
            entity = SpocEntity::class,
            parentColumns = arrayOf("id"),
            childColumns = arrayOf("spocId"),
            onDelete = ForeignKey.CASCADE,
        ),
    ],
    indices = [
        Index(value = ["spocId"], unique = false),
    ],
)
internal data class SpocImpressionEntity(
    val spocId: Int,
) {
    @PrimaryKey(autoGenerate = true)
    var impressionId: Int = 0
    var impressionDateInSeconds: Long = System.currentTimeMillis() / 1000
}

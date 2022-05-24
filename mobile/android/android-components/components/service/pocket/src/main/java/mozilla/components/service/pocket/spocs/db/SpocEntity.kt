/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.db

import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDatabase

/**
 * A sponsored Pocket story that is to be mapped to SQLite table.
 *
 * @property id Unique story id serving as the primary key of this entity.
 * @property url URL where the original story can be read.
 * @property title Title of the story.
 * @property imageUrl URL of the hero image for this story.
 * @property sponsor 3rd party sponsor of this story, e.g. "NextAdvisor".
 * @property clickShim Telemetry identifier for when the sponsored story is clicked.
 * @property impressionShim Telemetry identifier for when the sponsored story is seen by the user.
 * @property priority Priority level in deciding which stories to be shown first.
 * @property lifetimeCapCount Indicates how many times a sponsored story can be shown in total.
 * @property flightCapCount Indicates how many times a sponsored story can be shown within a period.
 * @property flightCapPeriod Indicates the period (number of seconds) in which at most [flightCapCount]
 * stories can be shown.
 */
@Entity(tableName = PocketRecommendationsDatabase.TABLE_NAME_SPOCS)
internal data class SpocEntity(
    @PrimaryKey
    val id: Int,
    val url: String,
    val title: String,
    val imageUrl: String,
    val sponsor: String,
    val clickShim: String,
    val impressionShim: String,
    val priority: Int,
    val lifetimeCapCount: Int,
    val flightCapCount: Int,
    val flightCapPeriod: Int,
)

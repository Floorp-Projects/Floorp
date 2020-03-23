/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.feature.share.db.RecentAppsDatabase.Companion.RECENT_APPS_TABLE

@Entity(
    tableName = RECENT_APPS_TABLE
)
internal data class RecentAppEntity(

    @PrimaryKey
    @ColumnInfo(name = "activityName")
    var activityName: String,

    @ColumnInfo(name = "score")
    var score: Double
)

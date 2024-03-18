/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey
import mozilla.components.browser.state.state.recover.TabState

/**
 * Internal entity representing recently closed tabs.
 */
@Entity(
    tableName = "recently_closed_tabs",
)
internal data class RecentlyClosedTabEntity(
    /**
     * Generated UUID for this closed tab.
     */
    @PrimaryKey
    @ColumnInfo(name = "uuid")
    var uuid: String,

    @ColumnInfo(name = "title")
    var title: String,

    @ColumnInfo(name = "url")
    var url: String,

    @ColumnInfo(name = "created_at")
    var createdAt: Long,
) {
    internal fun asTabState(): TabState {
        return TabState(
            id = uuid,
            title = title,
            url = url,
            lastAccess = createdAt,
        )
    }
}

internal fun TabState.toRecentlyClosedTabEntity(): RecentlyClosedTabEntity {
    return RecentlyClosedTabEntity(
        uuid = id,
        title = title,
        url = url,
        createdAt = lastAccess,
    )
}

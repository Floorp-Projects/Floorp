/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.db

import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert

/**
 * Internal DAO for accessing [TabEntity] instances.
 */
@Dao
internal interface TabDao {
    @Insert
    fun insertTab(tab: TabEntity): Long

    @Delete
    fun deleteTab(tab: TabEntity)
}

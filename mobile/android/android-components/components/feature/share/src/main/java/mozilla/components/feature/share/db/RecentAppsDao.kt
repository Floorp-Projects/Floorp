/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Update

@Dao
internal interface RecentAppsDao {

    @Insert
    fun insertRecentApp(recentApp: RecentAppEntity)

    @Query("""
        SELECT * FROM recent_apps_table 
        WHERE packageName = :packageName""")
    fun getRecentApp(packageName: String): RecentAppEntity?

    @Update
    fun updateRecentApp(recentApp: RecentAppEntity)

    @Update
    fun updateAllRecentApp(recentApps: List<RecentAppEntity>)

    @Query("""
        SELECT * FROM recent_apps_table
        WHERE score > 0 
        ORDER BY score DESC
        LIMIT :limit""")
    fun getRecentAppsUpTo(limit: Int): List<RecentAppEntity>

    @Query("""SELECT * FROM recent_apps_table""")
    fun getAllRecentApps(): List<RecentAppEntity>
}

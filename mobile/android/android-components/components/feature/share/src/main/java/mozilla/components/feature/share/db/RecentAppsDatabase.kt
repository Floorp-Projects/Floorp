/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for storing apps and their scores that determine if they are most recently used.
 */
@Database(entities = [RecentAppEntity::class], version = 1)
internal abstract class RecentAppsDatabase : RoomDatabase() {
    abstract fun recentAppsDao(): RecentAppsDao

    companion object {

        const val RECENT_APPS_TABLE = "RECENT_APPS_TABLE"

        @Volatile
        private var instance: RecentAppsDatabase? = null

        @Synchronized
        fun get(context: Context): RecentAppsDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                    context,
                    RecentAppsDatabase::class.java,
                    RECENT_APPS_TABLE
            ).build().also {
                instance = it
            }
        }
    }
}

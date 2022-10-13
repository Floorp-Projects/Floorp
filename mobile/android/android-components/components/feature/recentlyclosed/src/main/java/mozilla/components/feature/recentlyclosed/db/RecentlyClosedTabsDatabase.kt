/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for storing recently closed tabs.
 */
@Database(entities = [RecentlyClosedTabEntity::class], version = 1)
internal abstract class RecentlyClosedTabsDatabase : RoomDatabase() {
    abstract fun recentlyClosedTabDao(): RecentlyClosedTabDao

    companion object {
        @Volatile
        private var instance: RecentlyClosedTabsDatabase? = null

        @Synchronized
        fun get(context: Context): RecentlyClosedTabsDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                RecentlyClosedTabsDatabase::class.java,
                "recently_closed_tabs",
            ).build().also {
                instance = it
            }
        }
    }
}

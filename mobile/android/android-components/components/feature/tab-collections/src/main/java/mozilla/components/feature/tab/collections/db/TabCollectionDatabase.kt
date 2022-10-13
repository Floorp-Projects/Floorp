/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for storing collections and their tabs.
 */
@Database(entities = [TabCollectionEntity::class, TabEntity::class], version = 1)
internal abstract class TabCollectionDatabase : RoomDatabase() {
    abstract fun tabCollectionDao(): TabCollectionDao
    abstract fun tabDao(): TabDao

    companion object {
        @Volatile private var instance: TabCollectionDatabase? = null

        @Synchronized
        fun get(context: Context): TabCollectionDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                TabCollectionDatabase::class.java,
                "tab_collections",
            ).build().also {
                instance = it
            }
        }
    }
}

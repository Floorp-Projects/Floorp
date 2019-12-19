/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for storing top sites.
 */
@Database(entities = [TopSiteEntity::class], version = 1)
internal abstract class TopSiteDatabase : RoomDatabase() {
    abstract fun topSiteDao(): TopSiteDao

    companion object {
        @Volatile private var instance: TopSiteDatabase? = null

        @Synchronized
        fun get(context: Context): TopSiteDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                TopSiteDatabase::class.java,
                "top_sites"
            ).build().also {
                instance = it
            }
        }
    }
}

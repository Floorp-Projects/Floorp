/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase

/**
 * Internal database for storing top sites.
 */
@Database(entities = [TopSiteEntity::class], version = 2)
internal abstract class TopSiteDatabase : RoomDatabase() {
    abstract fun topSiteDao(): TopSiteDao

    companion object {
        @Volatile
        private var instance: TopSiteDatabase? = null

        @Synchronized
        fun get(context: Context): TopSiteDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                TopSiteDatabase::class.java,
                "top_sites"
            ).addMigrations(
                Migrations.migration_1_2
            ).build().also {
                instance = it
            }
        }
    }
}

internal object Migrations {
    val migration_1_2 = object : Migration(1, 2) {
        override fun migrate(database: SupportSQLiteDatabase) {
            // Add the new isDefault column and set isDefault to 0 (false) for every entry.
            database.execSQL(
                "ALTER TABLE top_sites ADD COLUMN isDefault INTEGER NOT NULL DEFAULT 0"
            )

            // Prior to version 2, pocket top sites, wikipedia and youtube were added as default
            // sites in Fenix. Look for these entries and set isDefault to 1 (true).
            database.execSQL(
                "UPDATE top_sites " +
                    "SET isDefault = 1 " +
                    "WHERE url IN " +
                    "('https://getpocket.com/fenix-top-articles', " +
                    "'https://www.wikipedia.org/', " +
                    "'https://www.youtube.com/')"
            )
        }
    }
}

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
@Database(entities = [PinnedSiteEntity::class], version = 3)
internal abstract class TopSiteDatabase : RoomDatabase() {
    abstract fun pinnedSiteDao(): PinnedSiteDao

    companion object {
        @Volatile
        private var instance: TopSiteDatabase? = null

        @Synchronized
        fun get(context: Context): TopSiteDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                TopSiteDatabase::class.java,
                "top_sites",
            ).addMigrations(
                Migrations.migration_1_2,
            ).addMigrations(
                Migrations.migration_2_3,
            ).build().also {
                instance = it
            }
        }
    }
}

internal object Migrations {
    val migration_1_2 = object : Migration(1, 2) {
        override fun migrate(database: SupportSQLiteDatabase) {
            // Add the new is_default column and set is_default to 0 (false) for every entry.
            database.execSQL(
                "ALTER TABLE top_sites ADD COLUMN is_default INTEGER NOT NULL DEFAULT 0",
            )

            // Prior to version 2, pocket top sites, wikipedia and youtube were added as default
            // sites in Fenix. Look for these entries and set is_default to 1 (true).
            database.execSQL(
                "UPDATE top_sites " +
                    "SET is_default = 1 " +
                    "WHERE url IN " +
                    "('https://getpocket.com/fenix-top-articles', " +
                    "'https://www.wikipedia.org/', " +
                    "'https://www.youtube.com/')",
            )
        }
    }

    @Suppress("MagicNumber")
    val migration_2_3 = object : Migration(2, 3) {
        override fun migrate(database: SupportSQLiteDatabase) {
            // Create a temporary top sites table of version 1.
            database.execSQL(
                "CREATE TABLE IF NOT EXISTS `top_sites_temp` (" +
                    "`id` INTEGER PRIMARY KEY AUTOINCREMENT, " +
                    "`title` TEXT NOT NULL, " +
                    "`url` TEXT NOT NULL, " +
                    "`is_default` INTEGER NOT NULL, " +
                    "`created_at` INTEGER NOT NULL)",
            )

            // Insert every entry from the old table into the temporary top sites table.
            database.execSQL(
                "INSERT INTO top_sites_temp (title, url, created_at, is_default) " +
                    "SELECT title, url, created_at, 0 FROM top_sites",
            )

            // Assume there are consumers of version 2 with the mismatched isDefault and is_default
            // column name. Drop the old table.
            database.execSQL(
                "DROP TABLE top_sites",
            )

            // Rename the temporary table to top_sites.
            database.execSQL(
                "ALTER TABLE top_sites_temp RENAME TO top_sites",
            )

            // Prior to version 2, pocket top sites, wikipedia and youtube were added as default
            // sites in Fenix. Look for these entries and set isDefault to 1 (true).
            database.execSQL(
                "UPDATE top_sites " +
                    "SET is_default = 1 " +
                    "WHERE url IN " +
                    "('https://getpocket.com/fenix-top-articles', " +
                    "'https://www.wikipedia.org/', " +
                    "'https://www.youtube.com/')",
            )
        }
    }
}

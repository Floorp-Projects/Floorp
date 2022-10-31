/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.share.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase
import mozilla.components.feature.share.db.RecentAppsDatabase.Companion.RECENT_APPS_TABLE

/**
 * Internal database for storing apps and their scores that determine if they are most recently used.
 */
@Database(entities = [RecentAppEntity::class], version = 2)
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
                RECENT_APPS_TABLE,
            ).addMigrations(
                Migrations.migration_1_2,
            ).build().also {
                instance = it
            }
        }
    }
}

internal object Migrations {
    val migration_1_2 = object : Migration(1, 2) {
        override fun migrate(database: SupportSQLiteDatabase) {
            database.execSQL("DROP TABLE RECENT_APPS_TABLE")
            database.execSQL(
                "CREATE TABLE IF NOT EXISTS " + RECENT_APPS_TABLE +
                    "(" +
                    "`activityName` TEXT NOT NULL, " +
                    "`score` DOUBLE NOT NULL, " +
                    " PRIMARY KEY(`activityName`)" +
                    ")",
            )
        }
    }
}

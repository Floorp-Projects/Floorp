/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.TypeConverter
import androidx.room.TypeConverters
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase
import mozilla.components.feature.sitepermissions.SitePermissions

/**
 * Internal database for saving site permissions.
 */
@Database(entities = [SitePermissionsEntity::class], version = 2)
@TypeConverters(StatusConverter::class)
internal abstract class SitePermissionsDatabase : RoomDatabase() {
    abstract fun sitePermissionsDao(): SitePermissionsDao

    companion object {
        @Volatile
        private var instance: SitePermissionsDatabase? = null

        @Synchronized
        fun get(context: Context): SitePermissionsDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                SitePermissionsDatabase::class.java,
                "site_permissions_database"
            ).addMigrations(
                Migrations.migration_1_2
            ).build().also { instance = it }
        }
    }
}

@Suppress("unused")
internal class StatusConverter {
    private val statusArray = SitePermissions.Status.values()

    @TypeConverter
    fun toInt(status: SitePermissions.Status): Int {
        return status.id
    }

    @TypeConverter
    fun toStatus(index: Int): SitePermissions.Status? {
        return statusArray.find { it.id == index }
    }
}

internal object Migrations {
    val migration_1_2 = object : Migration(1, 2) {
        override fun migrate(database: SupportSQLiteDatabase) {
            // Version 1 is used in Nightly builds of Fenix, but not in production. Let's just skip actually migrating
            // anything and let's re-create the "site_permissions" table.

            database.execSQL("DROP TABLE site_permissions")
            database.execSQL(
                "CREATE TABLE IF NOT EXISTS `site_permissions` (" +
                    "`origin` TEXT NOT NULL, " +
                    "`location` INTEGER NOT NULL, " +
                    "`notification` INTEGER NOT NULL, " +
                    "`microphone` INTEGER NOT NULL, " +
                    "`camera` INTEGER NOT NULL, " +
                    "`bluetooth` INTEGER NOT NULL, " +
                    "`local_storage` INTEGER NOT NULL, " +
                    "`saved_at` INTEGER NOT NULL," +
                    " PRIMARY KEY(`origin`))"
            )
        }
    }
}

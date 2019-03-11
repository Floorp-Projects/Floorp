/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling.db

import android.arch.persistence.db.SupportSQLiteDatabase
import android.arch.persistence.room.Database
import android.arch.persistence.room.Room
import android.arch.persistence.room.RoomDatabase
import android.arch.persistence.room.TypeConverter
import android.arch.persistence.room.TypeConverters
import android.arch.persistence.room.migration.Migration
import android.content.Context

/**
 * Internal database for saving bundles.
 */
@Database(entities = [BundleEntity::class], version = 2)
@TypeConverters(UrlListConverter::class)
internal abstract class BundleDatabase : RoomDatabase() {
    abstract fun bundleDao(): BundleDao

    companion object {
        @Volatile private var instance: BundleDatabase? = null

        @Synchronized
        fun get(context: Context): BundleDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                BundleDatabase::class.java,
                "bundle_database"
            ).addMigrations(
                Migrations.migration_1_2
            ).build().also {
                instance = it
            }
        }
    }
}

@Suppress("unused")
internal class UrlListConverter {
    @TypeConverter
    fun fromUrlList(urls: UrlList): String {
        return urls.entries.joinToString(separator = "\n")
    }

    @TypeConverter
    fun toUrlList(value: String): UrlList {
        return UrlList(value.split('\n'))
    }
}

internal object Migrations {
    val migration_1_2 = object : Migration(1, 2) {
        override fun migrate(database: SupportSQLiteDatabase) {
            // Version 1 is used in Nightly builds of Fenix, but not in production. Let's just skip actually migrating
            // anything and let's re-create the "bundles" table.

            database.execSQL("DROP TABLE bundles")
            database.execSQL("CREATE TABLE IF NOT EXISTS `bundles` (" +
                "`id` INTEGER PRIMARY KEY AUTOINCREMENT, " +
                "`saved_at` INTEGER NOT NULL, " +
                "`urls` TEXT NOT NULL, " +
                "`file` TEXT NOT NULL)"
            )
        }
    }
}

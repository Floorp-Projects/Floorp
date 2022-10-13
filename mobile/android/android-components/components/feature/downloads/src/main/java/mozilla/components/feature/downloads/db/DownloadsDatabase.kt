/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.TypeConverter
import androidx.room.TypeConverters
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase
import mozilla.components.browser.state.state.content.DownloadState

/**
 * Internal database for saving downloads.
 */
@Database(entities = [DownloadEntity::class], version = 4)
@TypeConverters(StatusConverter::class)
internal abstract class DownloadsDatabase : RoomDatabase() {
    abstract fun downloadDao(): DownloadDao

    companion object {
        @Volatile
        private var instance: DownloadsDatabase? = null

        @Synchronized
        fun get(context: Context): DownloadsDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                DownloadsDatabase::class.java,
                "mozac_downloads_database",
            ).addMigrations(
                Migrations.migration_1_2,
                Migrations.migration_2_3,
                Migrations.migration_3_4,
            ).build().also {
                instance = it
            }
        }
    }
}

@Suppress("MaxLineLength", "MagicNumber")
internal object Migrations {
    val migration_1_2 = object : Migration(1, 2) {
        override fun migrate(database: SupportSQLiteDatabase) {
            database.execSQL(
                "ALTER TABLE downloads ADD COLUMN is_private INTEGER NOT NULL DEFAULT 0",
            )
        }
    }
    val migration_2_3 = object : Migration(2, 3) {
        override fun migrate(database: SupportSQLiteDatabase) {
            // Create a temporal table
            database.execSQL("CREATE TABLE temp_downloads (`id` TEXT NOT NULL, `url` TEXT NOT NULL, `file_name` TEXT, `content_type` TEXT, `content_length` INTEGER, `status` INTEGER NOT NULL, `destination_directory` TEXT NOT NULL, `created_at` INTEGER NOT NULL, PRIMARY KEY(`id`))")
            // Copy the data
            database.execSQL("INSERT INTO temp_downloads (id,url,file_name,content_type,content_length,status,destination_directory,created_at) SELECT id,url,file_name,content_type,content_length,status,destination_directory,created_at FROM downloads where is_private = 0")
            // Remove the old table
            database.execSQL("DROP TABLE downloads")
            // Rename the table name to the correct one
            database.execSQL("ALTER TABLE temp_downloads RENAME TO downloads")
        }
    }

    val migration_3_4 = object : Migration(3, 4) {
        override fun migrate(database: SupportSQLiteDatabase) {
            // Clear any data urls.
            database.execSQL("UPDATE downloads SET url='' WHERE url LIKE 'data:%' ")
        }
    }
}

@Suppress("unused")
internal class StatusConverter {
    private val statusArray = DownloadState.Status.values()

    @TypeConverter
    fun toInt(status: DownloadState.Status): Int {
        return status.id
    }

    @TypeConverter
    fun toStatus(index: Int): DownloadState.Status? {
        return statusArray.find { it.id == index }
    }
}

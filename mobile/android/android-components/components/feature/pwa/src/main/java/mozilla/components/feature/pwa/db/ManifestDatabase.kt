/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.db

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.TypeConverters
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase

/**
 * Internal database for storing web app manifests and metadata.
 */
@Database(entities = [ManifestEntity::class], version = 3)
@TypeConverters(ManifestConverter::class)
internal abstract class ManifestDatabase : RoomDatabase() {
    abstract fun manifestDao(): ManifestDao

    @Suppress("MagicNumber")
    companion object {
        @Volatile private var instance: ManifestDatabase? = null

        @VisibleForTesting
        internal val MIGRATION_1_2: Migration = object : Migration(1, 2) {
            override fun migrate(database: SupportSQLiteDatabase) {
                val cursor = database.query("SELECT * FROM manifests LIMIT 0,1")

                if (cursor.getColumnIndex("used_at") < 0) {
                    database.execSQL("ALTER TABLE manifests ADD COLUMN used_at INTEGER NOT NULL DEFAULT 0")
                }

                if (cursor.getColumnIndex("scope") < 0) {
                    database.execSQL("ALTER TABLE manifests ADD COLUMN scope TEXT")
                }

                database.execSQL("CREATE INDEX IF NOT EXISTS index_manifests_scope ON manifests (scope)")
                database.execSQL("UPDATE manifests SET used_at = updated_at WHERE used_at = 0")
            }
        }

        @VisibleForTesting
        internal val MIGRATION_2_3: Migration = object : Migration(2, 3) {
            override fun migrate(database: SupportSQLiteDatabase) {
                database.execSQL("ALTER TABLE manifests ADD COLUMN has_share_targets INTEGER NOT NULL DEFAULT 0")

                database.execSQL(
                    "CREATE INDEX IF NOT EXISTS index_manifests_has_share_targets ON manifests (has_share_targets)",
                )
            }
        }

        @Synchronized
        fun get(context: Context): ManifestDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                ManifestDatabase::class.java,
                "manifests",
            ).addMigrations(MIGRATION_1_2, MIGRATION_2_3).build().also {
                instance = it
            }
        }
    }
}

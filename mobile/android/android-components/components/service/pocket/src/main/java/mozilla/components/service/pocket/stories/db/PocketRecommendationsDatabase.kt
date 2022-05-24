/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase
import mozilla.components.service.pocket.spocs.db.SpocEntity
import mozilla.components.service.pocket.spocs.db.SpocImpressionEntity
import mozilla.components.service.pocket.spocs.db.SpocsDao

/**
 * Internal database for storing Pocket items.
 */
@Database(
    entities = [
        PocketStoryEntity::class,
        SpocEntity::class,
        SpocImpressionEntity::class
    ],
    version = 3
)
internal abstract class PocketRecommendationsDatabase : RoomDatabase() {
    abstract fun pocketRecommendationsDao(): PocketRecommendationsDao
    abstract fun spocsDao(): SpocsDao

    companion object {
        private const val DATABASE_NAME = "pocket_recommendations"
        const val TABLE_NAME_STORIES = "stories"
        const val TABLE_NAME_SPOCS = "spocs"
        const val TABLE_NAME_SPOCS_IMPRESSIONS = "spocs_impressions"

        @Volatile
        private var instance: PocketRecommendationsDatabase? = null

        @Synchronized
        fun get(context: Context): PocketRecommendationsDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                PocketRecommendationsDatabase::class.java,
                DATABASE_NAME
            )
                .addMigrations(
                    Migrations.migration_1_2,
                    Migrations.migration_2_3,
                    Migrations.migration_1_3,
                )
                .build().also {
                    instance = it
                }
        }
    }
}

internal object Migrations {
    val migration_1_2 = object : Migration(1, 2) {
        override fun migrate(database: SupportSQLiteDatabase) {
            database.execSQL(
                "CREATE TABLE IF NOT EXISTS " +
                    "`${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}` (" +
                    "`url` TEXT NOT NULL, " +
                    "`title` TEXT NOT NULL, " +
                    "`imageUrl` TEXT NOT NULL, " +
                    "`sponsor` TEXT NOT NULL, " +
                    "`clickShim` TEXT NOT NULL, " +
                    "`impressionShim` TEXT NOT NULL, " +
                    "PRIMARY KEY(`url`)" +
                    ")"
            )
        }
    }

    /**
     * Migration for when adding support for pacing sponsored stories.
     */
    val migration_2_3 = object : Migration(2, 3) {
        override fun migrate(database: SupportSQLiteDatabase) {
            // There are many new columns added. Drop the old table allowing to start fresh.
            // This migration is expected to only be needed in debug builds
            // with the feature not being live in any Fenix release.
            database.execSQL(
                "DROP TABLE ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}"
            )

            database.createNewSpocsTables()
        }
    }

    /**
     * Migration for when adding sponsored stories along with pacing support.
     */
    val migration_1_3 = object : Migration(1, 3) {
        override fun migrate(database: SupportSQLiteDatabase) {
            database.createNewSpocsTables()
        }
    }

    private fun SupportSQLiteDatabase.createNewSpocsTables() {
        execSQL(
            "CREATE TABLE IF NOT EXISTS " +
                "`${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}` (" +
                "`id` INTEGER NOT NULL, " +
                "`url` TEXT NOT NULL, " +
                "`title` TEXT NOT NULL, " +
                "`imageUrl` TEXT NOT NULL, " +
                "`sponsor` TEXT NOT NULL, " +
                "`clickShim` TEXT NOT NULL, " +
                "`impressionShim` TEXT NOT NULL, " +
                "`priority` INTEGER NOT NULL, " +
                "`lifetimeCapCount` INTEGER NOT NULL, " +
                "`flightCapCount` INTEGER NOT NULL, " +
                "`flightCapPeriod` INTEGER NOT NULL, " +
                "PRIMARY KEY(`id`)" +
                ")"
        )

        execSQL(
            "CREATE TABLE IF NOT EXISTS " +
                "`${PocketRecommendationsDatabase.TABLE_NAME_SPOCS_IMPRESSIONS}` (" +
                "`spocId` INTEGER NOT NULL, " +
                "`impressionId` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, " +
                "`impressionDateInSeconds` INTEGER NOT NULL, " +
                "FOREIGN KEY(`spocId`) " +
                "REFERENCES `spocs`(`id`) " +
                "ON UPDATE NO ACTION ON DELETE CASCADE " +
                ")"
        )
    }
}

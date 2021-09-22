/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for storing Pocket items.
 */
@Database(entities = [PocketStoryEntity::class], version = 1)
internal abstract class PocketRecommendationsDatabase : RoomDatabase() {
    abstract fun pocketRecommendationsDao(): PocketRecommendationsDao

    companion object {
        private const val DATABASE_NAME = "pocket_recommendations"
        const val TABLE_NAME_STORIES = "stories"

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
                .build().also {
                    instance = it
                }
        }
    }
}

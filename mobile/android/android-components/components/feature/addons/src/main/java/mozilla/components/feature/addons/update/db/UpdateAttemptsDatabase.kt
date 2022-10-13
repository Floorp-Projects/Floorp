/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for saving site AddonUpdaterRequest.
 */
@Database(entities = [UpdateAttemptEntity::class], version = 1)
internal abstract class UpdateAttemptsDatabase : RoomDatabase() {
    abstract fun updateAttemptDao(): UpdateAttemptDao

    companion object {
        @Volatile
        private var instance: UpdateAttemptsDatabase? = null

        @Synchronized
        fun get(context: Context): UpdateAttemptsDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                UpdateAttemptsDatabase::class.java,
                "addons_updater_attempts_database",
            ).build().also { instance = it }
        }
    }
}

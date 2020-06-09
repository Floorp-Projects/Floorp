/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.containers.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for storing containers (contextual identities).
 */
@Database(entities = [ContainerEntity::class], version = 1)
internal abstract class ContainerDatabase : RoomDatabase() {
    abstract fun containerDao(): ContainerDao

    companion object {
        @Volatile
        private var instance: ContainerDatabase? = null

        @Synchronized
        fun get(context: Context): ContainerDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                ContainerDatabase::class.java,
                "containers"
            ).build().also {
                instance = it
            }
        }
    }
}

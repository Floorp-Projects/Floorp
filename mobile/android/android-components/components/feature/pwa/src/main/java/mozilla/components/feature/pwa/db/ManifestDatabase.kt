/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.TypeConverters

/**
 * Internal database for storing web app manifests and metadata.
 */
@Database(entities = [ManifestEntity::class], version = 1)
@TypeConverters(ManifestConverter::class)
internal abstract class ManifestDatabase : RoomDatabase() {
    abstract fun manifestDao(): ManifestDao

    companion object {
        @Volatile private var instance: ManifestDatabase? = null

        @Synchronized
        fun get(context: Context): ManifestDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                ManifestDatabase::class.java,
                "manifests"
            ).build().also {
                instance = it
            }
        }
    }
}

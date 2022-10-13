/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for storing collections and their tabs.
 */
@Database(
    entities = [CrashEntity::class, ReportEntity::class],
    version = 1,
)
internal abstract class CrashDatabase : RoomDatabase() {
    abstract fun crashDao(): CrashDao

    companion object {
        @Volatile private var instance: CrashDatabase? = null

        @Synchronized
        fun get(context: Context): CrashDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(context.applicationContext, CrashDatabase::class.java, "crashes")
                // We are allowing main thread queries here since we need to write to disk blocking
                // in a crash event before the process gets shutdown. At this point the app already
                // crashed and temporarily blocking the UI thread is not that problematic anymore.
                .allowMainThreadQueries()
                .build()
                .also {
                    instance = it
                }
        }
    }
}

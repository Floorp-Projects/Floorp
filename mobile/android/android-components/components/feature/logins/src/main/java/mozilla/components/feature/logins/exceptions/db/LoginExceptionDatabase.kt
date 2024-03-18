/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.logins.exceptions.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

/**
 * Internal database for storing login exceptions.
 */
@Database(entities = [LoginExceptionEntity::class], version = 1)
internal abstract class LoginExceptionDatabase : RoomDatabase() {
    abstract fun loginExceptionDao(): LoginExceptionDao

    companion object {
        @Volatile
        private var instance: LoginExceptionDatabase? = null

        @Synchronized
        fun get(context: Context): LoginExceptionDatabase {
            instance?.let { return it }

            return Room.databaseBuilder(
                context,
                LoginExceptionDatabase::class.java,
                "login_exceptions",
            ).build().also {
                instance = it
            }
        }
    }
}

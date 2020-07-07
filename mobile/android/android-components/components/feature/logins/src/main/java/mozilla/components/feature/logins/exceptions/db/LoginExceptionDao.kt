/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.logins.exceptions.db

import androidx.paging.DataSource
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Transaction
import kotlinx.coroutines.flow.Flow

/**
 * Internal DAO for accessing [LoginExceptionEntity] instances.
 */
@Dao
internal interface LoginExceptionDao {
    @Insert
    fun insertLoginException(exception: LoginExceptionEntity): Long

    @Delete
    fun deleteLoginException(exception: LoginExceptionEntity)

    @Transaction
    @Query("SELECT * FROM logins_exceptions")
    fun getLoginExceptions(): Flow<List<LoginExceptionEntity>>

    @Query("DELETE FROM logins_exceptions")
    fun deleteAllLoginExceptions()

    @Query("SELECT * FROM logins_exceptions WHERE origin = :origin")
    fun findExceptionByOrigin(origin: String): LoginExceptionEntity?

    @Transaction
    @Query("SELECT * FROM logins_exceptions")
    fun getLoginExceptionsPaged(): DataSource.Factory<Int, LoginExceptionEntity>
}

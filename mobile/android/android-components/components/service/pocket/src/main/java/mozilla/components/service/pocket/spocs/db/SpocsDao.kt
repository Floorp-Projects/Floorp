/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Transaction
import mozilla.components.service.pocket.stories.db.PocketRecommendationsDatabase

@Dao
internal interface SpocsDao {
    @Transaction
    suspend fun cleanOldAndInsertNewSpocs(spocs: List<SpocEntity>) {
        deleteAllSpocs()
        insertSpocs(spocs)
    }

    @Insert(onConflict = OnConflictStrategy.REPLACE) // Maybe some details changed
    suspend fun insertSpocs(spocs: List<SpocEntity>)

    @Query("DELETE FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}")
    suspend fun deleteAllSpocs()

    @Query("SELECT * FROM ${PocketRecommendationsDatabase.TABLE_NAME_SPOCS}")
    suspend fun getAllSpocs(): List<SpocEntity>
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.update.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query

/**
 * Internal dao for accessing and modifying add-on update requests in the database.
 */
@Dao
internal interface UpdateAttemptDao {
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    fun insertOrUpdate(entity: UpdateAttemptEntity): Long

    @Query("SELECT * FROM update_attempts where addon_id =:addonId")
    fun getUpdateAttemptFor(addonId: String): UpdateAttemptEntity?

    @Query("DELETE FROM update_attempts where addon_id =:addonId")
    fun deleteUpdateAttempt(addonId: String)
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session.bundling.db

import androidx.lifecycle.LiveData
import androidx.paging.DataSource
import androidx.room.Dao
import androidx.room.Delete
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Update

/**
 * Internal dao for accessing and modifying the bundles in the database.
 */
@Dao
internal interface BundleDao {
    @Insert
    fun insertBundle(bundle: BundleEntity): Long

    @Update
    fun updateBundle(bundle: BundleEntity)

    @Delete
    fun deleteBundle(bundle: BundleEntity)

    @Query("SELECT * FROM bundles WHERE saved_at >= :since ORDER BY saved_at DESC LIMIT :limit")
    fun getBundles(since: Long, limit: Int): LiveData<List<BundleEntity>>

    @Query("SELECT * FROM bundles WHERE saved_at >= :since")
    fun getBundlesPaged(since: Long): DataSource.Factory<Int, BundleEntity>

    @Query("SELECT * FROM bundles WHERE saved_at >= :since ORDER BY saved_at DESC LIMIT 1")
    fun getLastBundle(since: Long): BundleEntity?
}

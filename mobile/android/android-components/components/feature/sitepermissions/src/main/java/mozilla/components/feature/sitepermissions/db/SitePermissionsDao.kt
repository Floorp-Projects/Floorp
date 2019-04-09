/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sitepermissions.db

import android.arch.paging.DataSource
import android.arch.persistence.room.Dao
import android.arch.persistence.room.Delete
import android.arch.persistence.room.Insert
import android.arch.persistence.room.Query
import android.arch.persistence.room.Update

/**
 * Internal dao for accessing and modifying sitePermissions in the database.
 */
@Dao
internal interface SitePermissionsDao {

    @Insert
    fun insert(entity: SitePermissionsEntity): Long

    @Update
    fun update(entity: SitePermissionsEntity)

    @Query("SELECT * FROM site_permissions ORDER BY saved_at DESC")
    fun getSitePermissions(): List<SitePermissionsEntity>

    @Query("SELECT * FROM site_permissions where origin =:origin LIMIT 1")
    fun getSitePermissionsBy(origin: String): SitePermissionsEntity?

    @Delete
    fun deleteSitePermissions(entity: SitePermissionsEntity)

    @Query("DELETE FROM site_permissions")
    fun deleteAllSitePermissions()

    @Query("SELECT * FROM site_permissions ORDER BY saved_at DESC")
    fun getSitePermissionsPaged(): DataSource.Factory<Int, SitePermissionsEntity>
}

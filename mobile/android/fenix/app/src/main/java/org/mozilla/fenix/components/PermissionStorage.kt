/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.paging.DataSource
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Mockable
import kotlin.coroutines.CoroutineContext

@Mockable
class PermissionStorage(
    private val context: Context,
    @VisibleForTesting internal val dispatcher: CoroutineContext = Dispatchers.IO,
    @VisibleForTesting internal val permissionsStorage: SitePermissionsStorage =
        context.components.core.geckoSitePermissionsStorage
) {

    suspend fun add(sitePermissions: SitePermissions) = withContext(dispatcher) {
        permissionsStorage.save(sitePermissions)
    }

    suspend fun findSitePermissionsBy(origin: String): SitePermissions? = withContext(dispatcher) {
        permissionsStorage.findSitePermissionsBy(origin)
    }

    suspend fun updateSitePermissions(sitePermissions: SitePermissions) = withContext(dispatcher) {
        permissionsStorage.update(sitePermissions)
    }

    suspend fun getSitePermissionsPaged(): DataSource.Factory<Int, SitePermissions> {
        return permissionsStorage.getSitePermissionsPaged()
    }

    suspend fun deleteSitePermissions(sitePermissions: SitePermissions) = withContext(dispatcher) {
        permissionsStorage.remove(sitePermissions)
    }

    suspend fun deleteAllSitePermissions() = withContext(dispatcher) {
        permissionsStorage.removeAll()
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import androidx.paging.DataSource
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import org.mozilla.fenix.ext.components
import kotlin.coroutines.CoroutineContext

class PermissionStorage(
    private val context: Context,
    private val dispatcher: CoroutineContext = Dispatchers.IO,
    internal val permissionsStorage: SitePermissionsStorage =
        context.components.core.geckoSitePermissionsStorage,
) {

    /**
     * Persists the [sitePermissions] provided as a parameter.
     *
     * @param sitePermissions The [SitePermissions] to be stored.
     * @param private Indicates if the [SitePermissions] belongs to a private session.
     */
    suspend fun add(sitePermissions: SitePermissions, private: Boolean) = withContext(dispatcher) {
        permissionsStorage.save(sitePermissions, private = private)
    }

    /**
     * Finds all [SitePermissions] that match the [origin].
     *
     * @param origin The site to be used as filter in the search.
     * @param private Indicates if the [origin] belongs to a private session.
     */
    suspend fun findSitePermissionsBy(origin: String, private: Boolean): SitePermissions? =
        withContext(dispatcher) {
            permissionsStorage.findSitePermissionsBy(origin, private = private)
        }

    /**
     * Replaces an existing SitePermissions with the values of provided [sitePermissions].
     *
     * @param sitePermissions The [SitePermissions] to be updated.
     * @param private Indicates if the [SitePermissions] belongs to a private session.
     */
    suspend fun updateSitePermissions(sitePermissions: SitePermissions, private: Boolean) =
        withContext(dispatcher) {
            permissionsStorage.update(sitePermissions, private = private)
        }

    /**
     * Returns all saved [SitePermissions] instances as a [DataSource.Factory].
     *
     * A consuming app can transform the data source into a `LiveData<PagedList>` of when using RxJava2 into a
     * `Flowable<PagedList>` or `Observable<PagedList>`, that can be observed.
     *
     * - https://developer.android.com/topic/libraries/architecture/paging/data
     * - https://developer.android.com/topic/libraries/architecture/paging/ui
     */
    suspend fun getSitePermissionsPaged(): DataSource.Factory<Int, SitePermissions> {
        return permissionsStorage.getSitePermissionsPaged()
    }

    /**
     * Deletes all sitePermissions that match the sitePermissions provided as a parameter.
     * @param sitePermissions The [SitePermissions] to be deleted from the storage.
     */
    suspend fun deleteSitePermissions(sitePermissions: SitePermissions) = withContext(dispatcher) {
        permissionsStorage.remove(sitePermissions, private = false)
    }

    /**
     * Deletes all sitePermissions sitePermissions.
     */
    suspend fun deleteAllSitePermissions() = withContext(dispatcher) {
        permissionsStorage.removeAll()
    }
}

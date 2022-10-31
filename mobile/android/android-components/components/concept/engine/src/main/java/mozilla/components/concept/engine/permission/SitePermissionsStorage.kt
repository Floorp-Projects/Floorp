/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.permission

import androidx.paging.DataSource

/**
 * Represents a storage to store [SitePermissions].
 */
interface SitePermissionsStorage {
    /**
     * Persists the [sitePermissions] provided as a parameter.
     * @param sitePermissions the [sitePermissions] to be stored.
     * @param request the [PermissionRequest] to be stored, default to null.
     */
    suspend fun save(sitePermissions: SitePermissions, request: PermissionRequest? = null)

    /**
     * Saves the permission temporarily until the user navigates away.
     * @param request The requested permission to be save temporarily.
     */
    fun saveTemporary(request: PermissionRequest? = null) = Unit

    /**
     * Clears any temporary permissions.
     */
    fun clearTemporaryPermissions() = Unit

    /**
     * Replaces an existing SitePermissions with the values of [sitePermissions] provided as a parameter.
     * @param sitePermissions the sitePermissions to be updated.
     */
    suspend fun update(sitePermissions: SitePermissions)

    /**
     * Finds all SitePermissions that match the [origin].
     * @param origin the site to be used as filter in the search.
     */
    suspend fun findSitePermissionsBy(origin: String, includeTemporary: Boolean = false): SitePermissions?

    /**
     * Deletes all sitePermissions that match the sitePermissions provided as a parameter.
     * @param sitePermissions the sitePermissions to be deleted from the storage.
     */
    suspend fun remove(sitePermissions: SitePermissions)

    /**
     * Deletes all sitePermissions sitePermissions.
     */
    suspend fun removeAll()

    /**
     * Returns all sitePermissions in the store.
     */
    suspend fun all(): List<SitePermissions>

    /**
     * Returns all saved [SitePermissions] instances as a [DataSource.Factory].
     *
     * A consuming app can transform the data source into a `LiveData<PagedList>` of when using RxJava2 into a
     * `Flowable<PagedList>` or `Observable<PagedList>`, that can be observed.
     *
     * - https://developer.android.com/topic/libraries/architecture/paging/data
     * - https://developer.android.com/topic/libraries/architecture/paging/ui
     */
    suspend fun getSitePermissionsPaged(): DataSource.Factory<Int, SitePermissions>

    enum class Permission {
        MICROPHONE, BLUETOOTH, CAMERA, LOCAL_STORAGE, NOTIFICATION, LOCATION, AUTOPLAY_AUDIBLE,
        AUTOPLAY_INAUDIBLE, MEDIA_KEY_SYSTEM_ACCESS, STORAGE_ACCESS
    }
}

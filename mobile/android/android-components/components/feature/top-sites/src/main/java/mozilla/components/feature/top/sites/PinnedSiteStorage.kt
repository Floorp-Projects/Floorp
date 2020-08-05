/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import android.content.Context
import androidx.paging.DataSource
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import mozilla.components.feature.top.sites.adapter.PinnedSiteAdapter
import mozilla.components.feature.top.sites.db.TopSiteDatabase
import mozilla.components.feature.top.sites.db.PinnedSiteEntity

/**
 * A storage implementation for organizing pinned sites.
 */
class PinnedSiteStorage(
    context: Context
) {
    internal var database: Lazy<TopSiteDatabase> = lazy { TopSiteDatabase.get(context) }

    /**
     * Adds a new [PinnedSite].
     *
     * @param title The title string.
     * @param url The URL string.
     * @param isDefault Whether or not the pinned site added should be a default pinned site. This
     * is used to identify pinned sites that are added by the application.
     */
    fun addPinnedSite(title: String, url: String, isDefault: Boolean = false) {
        PinnedSiteEntity(
            title = title,
            url = url,
            isDefault = isDefault,
            createdAt = System.currentTimeMillis()
        ).also { entity ->
            entity.id = database.value.pinnedSiteDao().insertPinnedSite(entity)
        }
    }

    /**
     * Returns a [Flow] list of all the [PinnedSite] instances.
     */
    fun getPinnedSites(): Flow<List<PinnedSite>> {
        return database.value.pinnedSiteDao().getPinnedSites().map { list ->
            list.map { entity -> PinnedSiteAdapter(entity) }
        }
    }

    /**
     * Returns all [PinnedSite]s as a [DataSource.Factory].
     */
    fun getPinnedSitesPaged(): DataSource.Factory<Int, PinnedSite> = database.value
        .pinnedSiteDao()
        .getPinnedSitesPaged()
        .map { entity -> PinnedSiteAdapter(entity) }

    /**
     * Removes the given [PinnedSite].
     */
    fun removePinnedSite(site: PinnedSite) {
        val pinnedSiteEntity = (site as PinnedSiteAdapter).entity
        database.value.pinnedSiteDao().deletePinnedSite(pinnedSiteEntity)
    }
}

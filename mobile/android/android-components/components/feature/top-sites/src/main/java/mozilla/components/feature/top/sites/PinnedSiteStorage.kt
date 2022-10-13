/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.withContext
import mozilla.components.feature.top.sites.db.PinnedSiteEntity
import mozilla.components.feature.top.sites.db.TopSiteDatabase
import mozilla.components.feature.top.sites.db.toPinnedSite

/**
 * A storage implementation for organizing pinned sites.
 */
class PinnedSiteStorage(context: Context) {

    @VisibleForTesting
    internal var currentTimeMillis: () -> Long = { System.currentTimeMillis() }

    @VisibleForTesting
    internal var database: Lazy<TopSiteDatabase> = lazy { TopSiteDatabase.get(context) }
    private val pinnedSiteDao by lazy { database.value.pinnedSiteDao() }

    /**
     * Adds the given list pinned sites.
     *
     * @param topSites A list containing a title to url pair of top sites to be added.
     * @param isDefault Whether or not the pinned site added should be a default pinned site. This
     * is used to identify pinned sites that are added by the application.
     */
    suspend fun addAllPinnedSites(
        topSites: List<Pair<String, String>>,
        isDefault: Boolean = false,
    ) = withContext(IO) {
        val siteEntities = topSites.map { (title, url) ->
            PinnedSiteEntity(
                title = title,
                url = url,
                isDefault = isDefault,
                createdAt = currentTimeMillis(),
            )
        }
        pinnedSiteDao.insertAllPinnedSites(siteEntities)
    }

    /**
     * Adds a new pinned site.
     *
     * @param title The title string.
     * @param url The URL string.
     * @param isDefault Whether or not the pinned site added should be a default pinned site. This
     * is used to identify pinned sites that are added by the application.
     */
    suspend fun addPinnedSite(title: String, url: String, isDefault: Boolean = false) =
        withContext(IO) {
            val entity = PinnedSiteEntity(
                title = title,
                url = url,
                isDefault = isDefault,
                createdAt = currentTimeMillis(),
            )
            entity.id = pinnedSiteDao.insertPinnedSite(entity)
        }

    /**
     * Returns a list of all the pinned sites.
     */
    suspend fun getPinnedSites(): List<TopSite> = withContext(IO) {
        pinnedSiteDao.getPinnedSites().map { entity -> entity.toTopSite() }
    }

    /**
     * Removes the given pinned site.
     *
     * @param site The pinned site.
     */
    suspend fun removePinnedSite(site: TopSite) = withContext(IO) {
        pinnedSiteDao.deletePinnedSite(site.toPinnedSite())
    }

    /**
     * Updates the given pinned site.
     *
     * @param site The pinned site.
     * @param title The new title for the top site.
     * @param url The new url for the top site.
     */
    suspend fun updatePinnedSite(site: TopSite, title: String, url: String) = withContext(IO) {
        val pinnedSite = site.toPinnedSite()
        pinnedSite.title = title
        pinnedSite.url = url
        pinnedSiteDao.updatePinnedSite(pinnedSite)
    }

    /**
     * Returns a count of pinned sites.
     */
    suspend fun getPinnedSitesCount(): Int = withContext(IO) {
        pinnedSiteDao.getPinnedSitesCount()
    }
}

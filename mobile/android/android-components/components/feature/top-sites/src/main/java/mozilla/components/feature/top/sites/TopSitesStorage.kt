/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import mozilla.components.support.base.observer.Observable

/**
 * Abstraction layer above the [PinnedSiteStorage] and [PlacesHistoryStorage] storages.
 */
interface TopSitesStorage : Observable<TopSitesStorage.Observer> {
    /**
     * Adds a new top site.
     *
     * @param title The title string.
     * @param url The URL string.
     * @param isDefault Whether or not the pinned site added should be a default pinned site. This
     * is used to identify pinned sites that are added by the application.
     */
    fun addTopSite(title: String, url: String, isDefault: Boolean = false)

    /**
     * Removes the given [TopSite].
     *
     * @param topSite The top site.
     */
    fun removeTopSite(topSite: TopSite)

    /**
     * Return a unified list of top sites based on the given number of sites desired.
     * If `includeFrecent` is true, fill in any missing top sites with frecent top site results.
     *
     * @param totalSites A total number of sites that will be retrieve if possible.
     * @param includeFrecent If true, includes frecent top site results.
     */
    suspend fun getTopSites(totalSites: Int, includeFrecent: Boolean): List<TopSite>

    /**
     * Interface to be implemented by classes that want to observe the top site storage.
     */
    interface Observer {
        /**
         * Notify the observer when changes are made to the storage.
         */
        fun onStorageUpdated()
    }
}

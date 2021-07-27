/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.topsites

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSitesStorage
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import kotlin.coroutines.CoroutineContext

/**
 * Default implementation of [TopSitesStorage].
 *
 * @param pinnedSitesStorage An instance of [PinnedSiteStorage], used for storing pinned sites.
 */
class DefaultTopSitesStorage(
    private val pinnedSitesStorage: PinnedSiteStorage,
    coroutineContext: CoroutineContext = Dispatchers.IO
) : TopSitesStorage, Observable<TopSitesStorage.Observer> by ObserverRegistry() {

    private var scope = CoroutineScope(coroutineContext)

    override fun addTopSite(title: String, url: String, isDefault: Boolean) {
        scope.launch {
            pinnedSitesStorage.addPinnedSite(title, url, isDefault)

            notifyObservers { onStorageUpdated() }
        }
    }

    override fun removeTopSite(topSite: TopSite) {
        scope.launch {
            pinnedSitesStorage.removePinnedSite(topSite)

            notifyObservers { onStorageUpdated() }
        }
    }

    override fun updateTopSite(topSite: TopSite, title: String, url: String) {
        scope.launch {
            pinnedSitesStorage.updatePinnedSite(topSite, title, url)

            notifyObservers { onStorageUpdated() }
        }
    }

    override suspend fun getTopSites(
        totalSites: Int,
        frecencyConfig: FrecencyThresholdOption?
    ): List<TopSite> = pinnedSitesStorage.getPinnedSites().take(totalSites)

    companion object {
        const val TOP_SITES_MAX_LIMIT = 4
    }
}

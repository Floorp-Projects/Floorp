/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.feature.top.sites.ext.hasUrl
import mozilla.components.feature.top.sites.ext.toTopSite
import mozilla.components.feature.top.sites.facts.emitTopSitesCountFact
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import kotlin.coroutines.CoroutineContext

/**
 * Default implementation of [TopSitesStorage].
 *
 * @param pinnedSitesStorage An instance of [PinnedSiteStorage], used for storing pinned sites.
 * @param historyStorage An instance of [PlacesHistoryStorage], used for retrieving top frecent
 * sites from history.
 * @param topSitesProvider An optional instance of [TopSitesProvider], used for retrieving
 * additional top sites from a provider.
 * @param defaultTopSites A list containing a title to url pair of default top sites to be added
 * to the [PinnedSiteStorage].
 */
class DefaultTopSitesStorage(
    private val pinnedSitesStorage: PinnedSiteStorage,
    private val historyStorage: PlacesHistoryStorage,
    private val topSitesProvider: TopSitesProvider? = null,
    private val defaultTopSites: List<Pair<String, String>> = listOf(),
    coroutineContext: CoroutineContext = Dispatchers.IO
) : TopSitesStorage, Observable<TopSitesStorage.Observer> by ObserverRegistry() {

    private var scope = CoroutineScope(coroutineContext)

    // Cache of the last retrieved top sites
    var cachedTopSites = listOf<TopSite>()

    init {
        if (defaultTopSites.isNotEmpty()) {
            scope.launch {
                pinnedSitesStorage.addAllPinnedSites(defaultTopSites, isDefault = true)
            }
        }
    }

    override fun addTopSite(title: String, url: String, isDefault: Boolean) {
        scope.launch {
            pinnedSitesStorage.addPinnedSite(title, url, isDefault)
            notifyObservers { onStorageUpdated() }
        }
    }

    override fun removeTopSite(topSite: TopSite) {
        scope.launch {
            if (topSite is TopSite.Default || topSite is TopSite.Pinned) {
                pinnedSitesStorage.removePinnedSite(topSite)
            }

            // Remove the top site from both history and pinned sites storage to avoid having it
            // show up as a frecent site if it is a pinned site.
            historyStorage.deleteVisitsFor(topSite.url)

            notifyObservers { onStorageUpdated() }
        }
    }

    override fun updateTopSite(topSite: TopSite, title: String, url: String) {
        scope.launch {
            if (topSite is TopSite.Default || topSite is TopSite.Pinned) {
                pinnedSitesStorage.updatePinnedSite(topSite, title, url)
            }

            notifyObservers { onStorageUpdated() }
        }
    }

    override suspend fun getTopSites(
        totalSites: Int,
        frecencyConfig: FrecencyThresholdOption?
    ): List<TopSite> {
        val topSites = ArrayList<TopSite>()
        val pinnedSites = pinnedSitesStorage.getPinnedSites().take(totalSites)
        var numSitesRequired = totalSites - pinnedSites.size

        topSites.addAll(pinnedSites)

        topSitesProvider?.let { provider ->
            val providerTopSites = provider.getTopSites()
            topSites.addAll(providerTopSites.take(numSitesRequired))
            numSitesRequired -= providerTopSites.size
        }

        if (frecencyConfig != null && numSitesRequired > 0) {
            // Get 'totalSites' sites for duplicate entries with
            // existing pinned sites
            val frecentSites = historyStorage
                .getTopFrecentSites(totalSites, frecencyConfig)
                .map { it.toTopSite() }
                .filter { !pinnedSites.hasUrl(it.url) }
                .take(numSitesRequired)

            topSites.addAll(frecentSites)
        }

        emitTopSitesCountFact(pinnedSites.size)
        cachedTopSites = topSites

        return topSites
    }
}

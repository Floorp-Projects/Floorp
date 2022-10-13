/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.feature.top.sites.ext.hasUrl
import mozilla.components.feature.top.sites.ext.toTopSite
import mozilla.components.feature.top.sites.facts.emitTopSitesCountFact
import mozilla.components.support.base.log.logger.Logger
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
 * additional top sites from a provider. The returned top sites are added before pinned sites.
 * @param defaultTopSites A list containing a title to url pair of default top sites to be added
 * to the [PinnedSiteStorage].
 */
class DefaultTopSitesStorage(
    private val pinnedSitesStorage: PinnedSiteStorage,
    private val historyStorage: PlacesHistoryStorage,
    private val topSitesProvider: TopSitesProvider? = null,
    private val defaultTopSites: List<Pair<String, String>> = listOf(),
    coroutineContext: CoroutineContext = Dispatchers.IO,
) : TopSitesStorage, Observable<TopSitesStorage.Observer> by ObserverRegistry() {

    private var scope = CoroutineScope(coroutineContext)
    private val logger = Logger("DefaultTopSitesStorage")

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
            if (topSite !is TopSite.Provided) {
                historyStorage.deleteVisitsFor(topSite.url)
            }

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

    @Suppress("ComplexCondition", "TooGenericExceptionCaught")
    override suspend fun getTopSites(
        totalSites: Int,
        frecencyConfig: TopSitesFrecencyConfig?,
        providerConfig: TopSitesProviderConfig?,
    ): List<TopSite> {
        val topSites = ArrayList<TopSite>()
        val pinnedSites = pinnedSitesStorage.getPinnedSites().take(totalSites)
        var providerTopSites = emptyList<TopSite>()
        var numSitesRequired = totalSites - pinnedSites.size

        if (topSitesProvider != null &&
            providerConfig != null &&
            providerConfig.showProviderTopSites &&
            pinnedSites.size < providerConfig.maxThreshold
        ) {
            try {
                providerTopSites = topSitesProvider
                    .getTopSites(allowCache = true)
                    .filter { providerConfig.providerFilter?.invoke(it) ?: true }
                    .take(numSitesRequired)
                    .take(providerConfig.maxThreshold - pinnedSites.size)
                topSites.addAll(providerTopSites)
                numSitesRequired -= providerTopSites.size
            } catch (e: Exception) {
                logger.error("Failed to fetch top sites from provider", e)
            }
        }

        topSites.addAll(pinnedSites)

        if (frecencyConfig?.frecencyTresholdOption != null && numSitesRequired > 0) {
            // Get 'totalSites' sites for duplicate entries with
            // existing pinned sites
            val frecentSites = historyStorage
                .getTopFrecentSites(totalSites, frecencyConfig.frecencyTresholdOption)
                .map { it.toTopSite() }
                .filter {
                    !pinnedSites.hasUrl(it.url) &&
                        !providerTopSites.hasUrl(it.url) &&
                        frecencyConfig.frecencyFilter?.invoke(it) ?: true
                }
                .take(numSitesRequired)

            topSites.addAll(frecentSites)
        }

        emitTopSitesCountFact(pinnedSites.size)
        cachedTopSites = topSites

        return topSites
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.top.sites.presenter

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.feature.top.sites.TopSitesConfig
import mozilla.components.feature.top.sites.TopSitesStorage
import mozilla.components.feature.top.sites.view.TopSitesView
import kotlin.coroutines.CoroutineContext

/**
 * Default implementation of [TopSitesPresenter]. Connects the [TopSitesView] with the
 * [TopSitesStorage] to update the view whenever the storage is updated.
 *
 * @param view An implementor of [TopSitesView] that will be notified of changes to the storage.
 * @param storage The top sites storage that stores pinned and frecent sites.
 * @param config Lambda expression that returns [TopSitesConfig] which species the number of top
 * sites to return and whether or not to include frequently visited sites.
 */
internal class DefaultTopSitesPresenter(
    override val view: TopSitesView,
    override val storage: TopSitesStorage,
    private val config: () -> TopSitesConfig,
    coroutineContext: CoroutineContext = Dispatchers.IO,
) : TopSitesPresenter, TopSitesStorage.Observer {

    private val scope = CoroutineScope(coroutineContext)

    override fun start() {
        onStorageUpdated()

        storage.register(this)
    }

    override fun stop() {
        storage.unregister(this)
    }

    override fun onStorageUpdated() {
        val innerConfig = config.invoke()

        scope.launch {
            val topSites = storage.getTopSites(
                totalSites = innerConfig.totalSites,
                frecencyConfig = innerConfig.frecencyConfig,
                providerConfig = innerConfig.providerConfig,
            )

            scope.launch(Dispatchers.Main) {
                view.displayTopSites(topSites)
            }
        }
    }
}

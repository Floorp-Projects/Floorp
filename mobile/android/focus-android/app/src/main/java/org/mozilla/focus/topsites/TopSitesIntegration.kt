/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.topsites

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import mozilla.components.support.base.feature.LifecycleAwareFeature

class TopSitesIntegration(private val topSitesStorage: DefaultTopSitesStorage) : LifecycleAwareFeature {
    private var ioScope: CoroutineScope = CoroutineScope(Dispatchers.IO)

    fun deleteAllTopSites() {
        ioScope.launch {
            val topSitesList = topSitesStorage.getTopSites(
                totalSites = DefaultTopSitesStorage.TOP_SITES_MAX_LIMIT,
                frecencyConfig = null
            )
            topSitesList.forEach {
                topSitesStorage.removeTopSite(it)
            }
        }
    }

    override fun start() {
        // Do nothing
    }

    override fun stop() {
        ioScope.cancel()
    }
}

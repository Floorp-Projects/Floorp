/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import androidx.lifecycle.LiveData
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSiteStorage
import org.mozilla.fenix.test.Mockable

@Mockable
class TopSiteStorage(private val context: Context) {
    var cachedTopSites = listOf<TopSite>()

    private val topSiteStorage by lazy {
        TopSiteStorage(context)
    }

    /**
     * Adds a new [TopSite].
     */
    fun addTopSite(title: String, url: String) {
        topSiteStorage.addTopSite(title, url)
    }

    /**
     * Returns a [LiveData] list of all the [TopSite] instances.
     */
    fun getTopSites(): LiveData<List<TopSite>> {
        return topSiteStorage.getTopSites()
    }

    /**
     * Removes the given [TopSite].
     */
    fun removeTopSite(topSite: TopSite) {
        topSiteStorage.removeTopSite(topSite)
    }
}

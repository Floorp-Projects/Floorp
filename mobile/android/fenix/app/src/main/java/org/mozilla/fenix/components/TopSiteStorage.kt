/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import androidx.lifecycle.LiveData
import androidx.lifecycle.asLiveData
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSiteStorage
import mozilla.components.support.locale.LocaleManager
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.observeOnceAndRemoveObserver
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.advanced.getSelectedLocale
import org.mozilla.fenix.utils.Mockable

@Mockable
class TopSiteStorage(private val context: Context) {
    var cachedTopSites = listOf<TopSite>()

    val storage by lazy {
        TopSiteStorage(context)
    }

    init {
        addDefaultTopSites()
    }

    /**
     * Adds a new [TopSite].
     */
    fun addTopSite(title: String, url: String, isDefault: Boolean = false) {
        storage.addTopSite(title, url, isDefault)
    }

    /**
     * Returns a [LiveData] list of all the [TopSite] instances.
     */
    fun getTopSites(): LiveData<List<TopSite>> {
        return storage.getTopSites().asLiveData()
    }

    /**
     * Removes the given [TopSite].
     */
    fun removeTopSite(topSite: TopSite) {
        storage.removeTopSite(topSite)
    }

    private fun addDefaultTopSites() {
        val topSiteCandidates = mutableListOf<Pair<String, String>>()
        if (!context.settings().defaultTopSitesAdded) {
            topSiteCandidates.add(
                Pair(
                    context.getString(R.string.default_top_site_google),
                    SupportUtils.GOOGLE_URL
                )
            )

            if (LocaleManager.getSelectedLocale(context).language == "en") {
                topSiteCandidates.add(
                    Pair(
                        context.getString(R.string.pocket_pinned_top_articles),
                        SupportUtils.POCKET_TRENDING_URL
                    )
                )
            }

            topSiteCandidates.add(
                Pair(
                    context.getString(R.string.default_top_site_wikipedia),
                    SupportUtils.WIKIPEDIA_URL
                )
            )

            GlobalScope.launch(Dispatchers.IO) {
                topSiteCandidates.forEach { (title, url) ->
                    addTopSite(title, url, isDefault = true)
                }
            }
            context.settings().defaultTopSitesAdded = true
        }
    }

    /**
     * This is for issue https://github.com/mozilla-mobile/fenix/issues/11660. We prefetch the top
     * sites for startup so that we're sure that we have all the data available as our fragment is
     * launched to make sure that we can display everything on the home screen on the first drawing pass.
     * This method doesn't negatively affect performance since the [getTopSites] runs on the a
     * background thread.
     */
    fun prefetch() {
        getTopSites().observeOnceAndRemoveObserver {
            cachedTopSites = it
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import androidx.viewpager2.widget.ViewPager2
import kotlinx.android.synthetic.main.component_top_sites_pager.view.*
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.home.sessioncontrol.viewholders.topsites.TopSitesPagerAdapter

class TopSitePagerViewHolder(
    view: View,
    interactor: TopSiteInteractor
) : RecyclerView.ViewHolder(view) {

    private val topSitesPagerAdapter = TopSitesPagerAdapter(interactor)
    private val pageIndicator = view.page_indicator

    private val topSitesPageChangeCallback = object : ViewPager2.OnPageChangeCallback() {
        override fun onPageSelected(position: Int) {
            pageIndicator.setSelection(position)
        }
    }

    init {
        view.top_sites_pager.apply {
            adapter = topSitesPagerAdapter
            registerOnPageChangeCallback(topSitesPageChangeCallback)
        }
    }

    fun bind(topSites: List<TopSite>) {
        topSitesPagerAdapter.updateData(topSites)

        // Don't show any page indicator if there is only 1 page.
        val numPages = if (topSites.size > TOP_SITES_PER_PAGE) {
            TOP_SITES_MAX_PAGE_SIZE
        } else {
            0
        }

        pageIndicator.isVisible = numPages > 1
        pageIndicator.setSize(numPages)
    }

    companion object {
        const val LAYOUT_ID = R.layout.component_top_sites_pager
        const val TOP_SITES_MAX_PAGE_SIZE = 2
        const val TOP_SITES_PER_PAGE = 8
    }
}

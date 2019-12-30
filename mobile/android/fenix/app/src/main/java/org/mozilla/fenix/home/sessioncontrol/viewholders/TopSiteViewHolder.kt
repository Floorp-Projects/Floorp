/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_top_sites.view.*
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.R
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.home.sessioncontrol.viewholders.topsites.TopSitesAdapter

class TopSiteViewHolder(
    view: View,
    interactor: TopSiteInteractor,
    override val containerView: View? = view
) : RecyclerView.ViewHolder(view), LayoutContainer {
    private val topSitesAdapter = TopSitesAdapter(interactor)

    init {
        view.top_sites_list.apply {
            adapter = topSitesAdapter
            layoutManager = GridLayoutManager(view.context, NUM_COLUMNS)
        }
    }

    fun bind(topSites: List<TopSite>) {
        topSitesAdapter.submitList(topSites)
    }

    companion object {
        const val LAYOUT_ID = R.layout.component_top_sites
        const val NUM_COLUMNS = 5
    }
}

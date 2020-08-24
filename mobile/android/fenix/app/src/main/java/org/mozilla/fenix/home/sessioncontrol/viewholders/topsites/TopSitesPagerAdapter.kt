/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.topsites

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.home.sessioncontrol.viewholders.TopSiteViewHolder

class TopSitesPagerAdapter(
    private val interactor: TopSiteInteractor
) : RecyclerView.Adapter<TopSiteViewHolder>() {

    private var topSites: List<List<TopSite>> = listOf()

    fun updateData(topSites: List<TopSite>) {
        this.topSites = topSites.chunked(TOP_SITES_PER_PAGE)
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TopSiteViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(TopSiteViewHolder.LAYOUT_ID, parent, false)
        return TopSiteViewHolder(view, interactor)
    }

    override fun onBindViewHolder(holder: TopSiteViewHolder, position: Int) {
        holder.bind(this.topSites[position])
    }

    override fun getItemCount(): Int = this.topSites.size

    companion object {
        const val TOP_SITES_PER_PAGE = 8
    }
}

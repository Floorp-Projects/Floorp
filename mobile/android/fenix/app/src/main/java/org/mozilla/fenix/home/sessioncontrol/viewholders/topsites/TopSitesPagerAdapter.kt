/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.topsites

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import kotlinx.android.synthetic.main.component_top_sites.view.*
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.home.sessioncontrol.viewholders.TopSiteViewHolder

class TopSitesPagerAdapter(
    private val interactor: TopSiteInteractor
) : ListAdapter<List<TopSite>, TopSiteViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TopSiteViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(TopSiteViewHolder.LAYOUT_ID, parent, false)
        return TopSiteViewHolder(view, interactor)
    }

    override fun onBindViewHolder(holder: TopSiteViewHolder, position: Int) {
        val adapter = holder.itemView.top_sites_list.adapter as TopSitesAdapter
        adapter.submitList(getItem(position))
    }

    private object DiffCallback : DiffUtil.ItemCallback<List<TopSite>>() {
        override fun areItemsTheSame(oldItem: List<TopSite>, newItem: List<TopSite>): Boolean {
            return oldItem.size == newItem.size
        }

        override fun areContentsTheSame(oldItem: List<TopSite>, newItem: List<TopSite>): Boolean {
            return newItem.zip(oldItem).all { (new, old) -> new == old }
        }
    }
}

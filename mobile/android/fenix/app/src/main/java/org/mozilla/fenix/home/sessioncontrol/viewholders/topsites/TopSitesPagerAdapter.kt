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
import org.mozilla.fenix.home.sessioncontrol.AdapterItem.TopSitePagerPayload
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.home.sessioncontrol.viewholders.TopSitePagerViewHolder.Companion.TOP_SITES_PER_PAGE
import org.mozilla.fenix.home.sessioncontrol.viewholders.TopSiteViewHolder

class TopSitesPagerAdapter(
    private val interactor: TopSiteInteractor
) : ListAdapter<List<TopSite>, TopSiteViewHolder>(TopSiteListDiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TopSiteViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(TopSiteViewHolder.LAYOUT_ID, parent, false)
        return TopSiteViewHolder(view, interactor)
    }

    override fun onBindViewHolder(
        holder: TopSiteViewHolder,
        position: Int,
        payloads: MutableList<Any>
    ) {
        if (payloads.isNullOrEmpty()) {
            onBindViewHolder(holder, position)
        } else {
            if (payloads[0] is TopSitePagerPayload) {
                val adapter = holder.itemView.top_sites_list.adapter as TopSitesAdapter
                val payload = payloads[0] as TopSitePagerPayload

                update(payload, position, adapter)
            }
        }
    }

    private fun update(
        payload: TopSitePagerPayload,
        position: Int,
        adapter: TopSitesAdapter
    ) {
        // Only currently selected page items need to be updated.
        for (item in payload.changed) {
            if (item.first < TOP_SITES_PER_PAGE && position == 0) {
                adapter.notifyItemChanged(item.first, item.second)
            } else if (item.first >= TOP_SITES_PER_PAGE && position == 1) {
                adapter.notifyItemChanged(item.first - TOP_SITES_PER_PAGE, item.second)
            }
        }
    }

    override fun onBindViewHolder(holder: TopSiteViewHolder, position: Int) {
        val adapter = holder.itemView.top_sites_list.adapter as TopSitesAdapter
        adapter.submitList(getItem(position))
    }

    internal object TopSiteListDiffCallback : DiffUtil.ItemCallback<List<TopSite>>() {
        override fun areItemsTheSame(oldItem: List<TopSite>, newItem: List<TopSite>): Boolean {
            return oldItem.size == newItem.size
        }

        override fun areContentsTheSame(oldItem: List<TopSite>, newItem: List<TopSite>): Boolean {
            return newItem.zip(oldItem).all { (new, old) -> new == old }
        }

        override fun getChangePayload(oldItem: List<TopSite>, newItem: List<TopSite>): Any? {
            val changed = mutableSetOf<Pair<Int, TopSite>>()
            for ((index, item) in newItem.withIndex()) {
                if (oldItem.getOrNull(index) != item) {
                    changed.add(Pair(index, item))
                }
            }
            return if (changed.isNotEmpty()) TopSitePagerPayload(changed) else null
        }
    }
}

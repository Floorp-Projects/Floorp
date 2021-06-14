/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.topsites

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.perf.StartupTimeline

class TopSitesAdapter(
    private val interactor: TopSiteInteractor
) : ListAdapter<TopSite, TopSiteItemViewHolder>(TopSitesDiffCallback) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TopSiteItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(TopSiteItemViewHolder.LAYOUT_ID, parent, false)
        return TopSiteItemViewHolder(view, interactor)
    }

    override fun onBindViewHolder(holder: TopSiteItemViewHolder, position: Int) {
        StartupTimeline.onTopSitesItemBound(holder)
        holder.bind(getItem(position))
    }

    override fun onBindViewHolder(
        holder: TopSiteItemViewHolder,
        position: Int,
        payloads: MutableList<Any>
    ) {
        if (payloads.isNullOrEmpty()) {
            onBindViewHolder(holder, position)
        } else {
            when (payloads[0]) {
                is TopSite -> {
                    holder.bind((payloads[0] as TopSite))
                }
            }
        }
    }

    internal object TopSitesDiffCallback : DiffUtil.ItemCallback<TopSite>() {
        override fun areItemsTheSame(oldItem: TopSite, newItem: TopSite) = oldItem.id == newItem.id

        override fun areContentsTheSame(oldItem: TopSite, newItem: TopSite) =
            oldItem.id == newItem.id && oldItem.title == newItem.title && oldItem.url == newItem.url

        override fun getChangePayload(oldItem: TopSite, newItem: TopSite): Any? {
            return if (oldItem.id == newItem.id && oldItem.url == newItem.url && oldItem.title != newItem.title) {
                newItem
            } else null
        }
    }
}

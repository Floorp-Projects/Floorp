/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.home.sessioncontrol.AdapterItem.TopSitePagerPayload
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.home.topsites.TopSitePagerViewHolder.Companion.TOP_SITES_PER_PAGE

class TopSitesPagerAdapter(
    private val appStore: AppStore,
    private val viewLifecycleOwner: LifecycleOwner,
    private val interactor: TopSiteInteractor,
) : ListAdapter<List<TopSite>, TopSiteViewHolder>(TopSiteListDiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TopSiteViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(TopSiteViewHolder.LAYOUT_ID, parent, false)
        return TopSiteViewHolder(view, appStore, viewLifecycleOwner, interactor)
    }

    override fun onBindViewHolder(
        holder: TopSiteViewHolder,
        position: Int,
        payloads: MutableList<Any>,
    ) {
        if (payloads.isNullOrEmpty()) {
            onBindViewHolder(holder, position)
        } else {
            if (payloads[0] is TopSitePagerPayload) {
                val adapter = holder.binding.topSitesList.adapter as TopSitesAdapter
                val payload = payloads[0] as TopSitePagerPayload

                update(payload, position, adapter)
            }
        }
    }

    @VisibleForTesting
    internal fun update(
        payload: TopSitePagerPayload,
        position: Int,
        adapter: TopSitesAdapter,
    ) {
        // Only currently selected page items need to be updated
        val currentPageChangedItems = getCurrentPageChanges(payload, position)

        // If no changes have been made to the current page no need to continue
        if (currentPageChangedItems.isEmpty()) return

        // Build the new list from the old one
        val refreshedItems: MutableList<TopSite> = mutableListOf()
        refreshedItems.addAll(adapter.currentList)

        // Update new list with the changed items
        currentPageChangedItems.forEach { item ->
            val index = item.first - (position * TOP_SITES_PER_PAGE)
            if (index in refreshedItems.indices) {
                refreshedItems[index] = item.second
            }
        }

        // Display the updated list without any of the removed items
        adapter.submitList(refreshedItems.filter { it.id != -1L })
    }

    /**
     * @returns the changed only items for the currently specified page in [position]
     */
    @VisibleForTesting
    internal fun getCurrentPageChanges(payload: TopSitePagerPayload, position: Int) =
        payload.changed.filter { changedPair ->
            if (position == 0) {
                changedPair.first < TOP_SITES_PER_PAGE
            } else {
                changedPair.first >= TOP_SITES_PER_PAGE
            }
        }

    override fun onBindViewHolder(holder: TopSiteViewHolder, position: Int) {
        val adapter = holder.binding.topSitesList.adapter as TopSitesAdapter
        adapter.submitList(getItem(position))
    }

    internal object TopSiteListDiffCallback : DiffUtil.ItemCallback<List<TopSite>>() {
        override fun areItemsTheSame(oldItem: List<TopSite>, newItem: List<TopSite>): Boolean {
            return oldItem.size == newItem.size
        }

        override fun areContentsTheSame(oldItem: List<TopSite>, newItem: List<TopSite>): Boolean {
            @Suppress("DiffUtilEquals")
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

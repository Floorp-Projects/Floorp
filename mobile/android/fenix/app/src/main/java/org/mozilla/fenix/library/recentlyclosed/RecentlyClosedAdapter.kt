/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.browser.state.state.recover.TabState
import org.mozilla.fenix.selection.SelectionHolder

class RecentlyClosedAdapter(
    private val interactor: RecentlyClosedFragmentInteractor,
) : ListAdapter<TabState, RecentlyClosedItemViewHolder>(DiffCallback),
    SelectionHolder<TabState> {

    private var selectedTabs: Set<TabState> = emptySet()

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int,
    ): RecentlyClosedItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(RecentlyClosedItemViewHolder.LAYOUT_ID, parent, false)
        return RecentlyClosedItemViewHolder(view, interactor, this)
    }

    override fun onBindViewHolder(holder: RecentlyClosedItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    override val selectedItems: Set<TabState>
        get() = selectedTabs

    fun updateData(tabs: List<TabState>, selectedTabs: Set<TabState>) {
        this.selectedTabs = selectedTabs
        notifyItemRangeChanged(0, tabs.size)
        submitList(tabs)
    }

    private object DiffCallback : DiffUtil.ItemCallback<TabState>() {
        override fun areItemsTheSame(oldItem: TabState, newItem: TabState) =
            oldItem.id == newItem.id

        override fun areContentsTheSame(oldItem: TabState, newItem: TabState) =
            oldItem == newItem
    }
}

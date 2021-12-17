/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.view

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.historymetadata.interactor.HistoryMetadataGroupInteractor
import org.mozilla.fenix.selection.SelectionHolder

/**
 * Adapter for a list of history metadata items to be displayed.
 */
class HistoryMetadataGroupAdapter(
    private val interactor: HistoryMetadataGroupInteractor
) : ListAdapter<History.Metadata, HistoryMetadataGroupItemViewHolder>(DiffCallback),
    SelectionHolder<History.Metadata> {

    private var selectedHistoryItems: Set<History.Metadata> = emptySet()

    override val selectedItems: Set<History.Metadata>
        get() = selectedHistoryItems

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): HistoryMetadataGroupItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(HistoryMetadataGroupItemViewHolder.LAYOUT_ID, parent, false)
        return HistoryMetadataGroupItemViewHolder(view, interactor, this)
    }

    override fun onBindViewHolder(holder: HistoryMetadataGroupItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    fun updateData(items: List<History.Metadata>) {
        this.selectedHistoryItems = items.filter { it.selected }.toSet()
        notifyItemRangeChanged(0, items.size)
        submitList(items)
    }

    internal object DiffCallback : DiffUtil.ItemCallback<History.Metadata>() {
        override fun areContentsTheSame(oldItem: History.Metadata, newItem: History.Metadata): Boolean =
            oldItem.position == newItem.position

        override fun areItemsTheSame(oldItem: History.Metadata, newItem: History.Metadata): Boolean =
            oldItem == newItem
    }
}

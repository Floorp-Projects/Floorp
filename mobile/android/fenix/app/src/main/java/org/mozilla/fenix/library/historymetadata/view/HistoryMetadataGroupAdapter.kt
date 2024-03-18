/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.view

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.PendingDeletionHistory
import org.mozilla.fenix.library.historymetadata.interactor.HistoryMetadataGroupInteractor
import org.mozilla.fenix.selection.SelectionHolder

/**
 * Adapter for a list of history metadata items to be displayed.
 */
class HistoryMetadataGroupAdapter(
    private val interactor: HistoryMetadataGroupInteractor,
    private val onEmptyStateChanged: (Boolean) -> Unit,
) : ListAdapter<History.Metadata, HistoryMetadataGroupItemViewHolder>(DiffCallback),
    SelectionHolder<History.Metadata> {

    private var selectedHistoryItems: Set<History.Metadata> = emptySet()
    private var pendingDeletionItems = emptySet<PendingDeletionHistory>()
    private var isEmpty = true

    override val selectedItems: Set<History.Metadata>
        get() = selectedHistoryItems

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int,
    ): HistoryMetadataGroupItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(HistoryMetadataGroupItemViewHolder.LAYOUT_ID, parent, false)
        return HistoryMetadataGroupItemViewHolder(view, interactor, this)
    }

    override fun getItemId(position: Int): Long {
        return getItem(position).visitedAt
    }

    override fun onBindViewHolder(holder: HistoryMetadataGroupItemViewHolder, position: Int) {
        val current = getItem(position) ?: return
        if (position == 0) {
            isEmpty = true
        }

        val isPendingDeletion = pendingDeletionItems.any {
            it is PendingDeletionHistory.MetaData &&
                it.key == current.historyMetadataKey &&
                it.visitedAt == current.visitedAt
        }

        // If there is a single visible item, it's enough to change the empty state of the view.
        if (isEmpty && !isPendingDeletion) {
            isEmpty = false
            onEmptyStateChanged.invoke(isEmpty)
        } else if (position + 1 == itemCount) {
            // If we reached the bottom of the list and there still has been zero visible items,
            // we can can change the History Group view state to empty.
            if (isEmpty) {
                onEmptyStateChanged.invoke(isEmpty)
            }
        }

        holder.bind(getItem(position), isPendingDeletion)
    }

    fun updateData(items: List<History.Metadata>) {
        submitList(items)
    }

    /**
     * @param selectedHistoryItems is used to keep track of the items selected by the user.
     */
    fun updateSelectedItems(selectedHistoryItems: Set<History.Metadata>) {
        this.selectedHistoryItems = selectedHistoryItems
    }

    /**
     * @param pendingDeletionItems is used to keep track of the items selected by the user.
     */
    fun updatePendingDeletionItems(pendingDeletionItems: Set<PendingDeletionHistory>) {
        this.pendingDeletionItems = pendingDeletionItems
    }

    internal object DiffCallback : DiffUtil.ItemCallback<History.Metadata>() {
        override fun areContentsTheSame(oldItem: History.Metadata, newItem: History.Metadata): Boolean =
            oldItem.position == newItem.position

        override fun areItemsTheSame(oldItem: History.Metadata, newItem: History.Metadata): Boolean =
            oldItem == newItem
    }
}

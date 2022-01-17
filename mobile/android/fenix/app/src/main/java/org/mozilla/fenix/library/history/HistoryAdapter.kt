/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.paging.PagedListAdapter
import androidx.recyclerview.widget.DiffUtil
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.library.history.viewholders.HistoryListItemViewHolder

class HistoryAdapter(
    private val historyInteractor: HistoryInteractor,
) : PagedListAdapter<History, HistoryListItemViewHolder>(historyDiffCallback),
    SelectionHolder<History> {

    private var mode: HistoryFragmentState.Mode = HistoryFragmentState.Mode.Normal
    override val selectedItems get() = mode.selectedItems
    var pendingDeletionIds = emptySet<Long>()
    private val itemsWithHeaders: MutableMap<HistoryItemTimeGroup, Int> = mutableMapOf()

    override fun getItemViewType(position: Int): Int = HistoryListItemViewHolder.LAYOUT_ID

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): HistoryListItemViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return HistoryListItemViewHolder(view, historyInteractor, this)
    }

    fun updateMode(mode: HistoryFragmentState.Mode) {
        this.mode = mode
        // Update the delete button alpha that the first item holds
        if (itemCount > 0) notifyItemChanged(0)
    }

    override fun onBindViewHolder(holder: HistoryListItemViewHolder, position: Int) {
        val current = getItem(position) ?: return
        val isPendingDeletion = pendingDeletionIds.contains(current.visitedAt)
        var timeGroup: HistoryItemTimeGroup? = null

        // Add or remove the header and position to the map depending on it's deletion status
        if (itemsWithHeaders.containsKey(current.historyTimeGroup)) {
            if (isPendingDeletion && itemsWithHeaders[current.historyTimeGroup] == position) {
                itemsWithHeaders.remove(current.historyTimeGroup)
            } else if (isPendingDeletion && itemsWithHeaders[current.historyTimeGroup] != position) {
                // do nothing
            } else {
                if (position <= itemsWithHeaders[current.historyTimeGroup] as Int) {
                    itemsWithHeaders[current.historyTimeGroup] = position
                    timeGroup = current.historyTimeGroup
                }
            }
        } else if (!isPendingDeletion) {
            itemsWithHeaders[current.historyTimeGroup] = position
            timeGroup = current.historyTimeGroup
        }

        holder.bind(current, timeGroup, position == 0, mode, isPendingDeletion)
    }

    fun updatePendingDeletionIds(pendingDeletionIds: Set<Long>) {
        this.pendingDeletionIds = pendingDeletionIds
    }

    companion object {
        private val historyDiffCallback = object : DiffUtil.ItemCallback<History>() {
            override fun areItemsTheSame(oldItem: History, newItem: History): Boolean {
                return oldItem == newItem
            }

            override fun areContentsTheSame(oldItem: History, newItem: History): Boolean {
                return oldItem == newItem
            }

            override fun getChangePayload(oldItem: History, newItem: History): Any? {
                return newItem
            }
        }
    }
}

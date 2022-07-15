/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.content.res.Resources
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.HistoryListGroupBinding
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryAdapter
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryInteractor
import org.mozilla.fenix.library.history.HistoryViewItem
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.library.LibrarySiteItemView

/**
 * A view representing a search group (a group of history items) in the history list.
 * [HistoryAdapter] is responsible for creating and populating the view.
 *
 * @param view that is passed down to the parent's constructor.
 * @param historyInteractor Passed down to [LibrarySiteItemView], to handle selection of multiple items.
 * @param selectionHolder Contains selected elements.
 * @param onDeleteClicked Invokes when a delete button is pressed.
 */
class HistoryGroupViewHolder(
    view: View,
    private val historyInteractor: HistoryInteractor,
    private val selectionHolder: SelectionHolder<History>,
    private val onDeleteClicked: (Int) -> Unit
) : RecyclerView.ViewHolder(view) {

    private val binding = HistoryListGroupBinding.bind(view)

    init {
        binding.historyGroupLayout.overflowView.apply {
            setImageResource(R.drawable.ic_close)
            contentDescription = view.context.getString(R.string.history_delete_item)
            setOnClickListener {
                onDeleteClicked.invoke(bindingAdapterPosition)
            }
        }
    }

    /**
     * Binds data to the view.
     *
     * @param item Data associated with the view.
     * @param mode is used to determine if the list is in the multiple-selection state or not.
     * @param groupPendingDeletionCount is used to adjust the number of items inside a group,
     * based on the number of items the user has removed from it.
     */
    fun bind(
        item: HistoryViewItem.HistoryGroupItem,
        mode: HistoryFragmentState.Mode,
        groupPendingDeletionCount: Int
    ) {
        with(binding.historyGroupLayout) {
            iconView.setImageResource(R.drawable.ic_multiple_tabs)

            titleView.text = item.data.title
            urlView.text = getGroupCountText(
                itemSize = item.data.items.size,
                pendingDeletionSize = groupPendingDeletionCount,
                resources = resources
            )

            setSelectionInteractor(item.data, selectionHolder, historyInteractor)
            changeSelected(item.data in selectionHolder.selectedItems)

            if (mode is HistoryFragmentState.Mode.Editing) {
                overflowView.hideAndDisable()
            } else {
                overflowView.showAndEnable()
            }
        }
    }

    internal fun getGroupCountText(
        itemSize: Int,
        pendingDeletionSize: Int,
        resources: Resources
    ): String {
        val numChildren = itemSize - pendingDeletionSize
        val stringId = if (numChildren == 1) {
            R.string.history_search_group_site
        } else {
            R.string.history_search_group_sites
        }
        return String.format(resources.getString(stringId), numChildren)
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_list_group
    }
}

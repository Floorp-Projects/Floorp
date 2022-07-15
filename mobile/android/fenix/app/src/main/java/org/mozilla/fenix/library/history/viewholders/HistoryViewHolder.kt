/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.HistoryListHistoryBinding
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryAdapter
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryInteractor
import org.mozilla.fenix.library.history.HistoryViewItem
import org.mozilla.fenix.selection.SelectionHolder

/**
 * A view representing a regular history record in the history and synced history lists.
 * [HistoryAdapter] is responsible for creating and populating the view.
 *
 * @param view that is passed down to the parent's constructor.
 * @param historyInteractor Passed down to [LibrarySiteItemView], to handle selection of multiple items.
 * @param selectionHolder Contains selected elements.
 * @param onDeleteClicked Invokes when a delete button is pressed.
 */
class HistoryViewHolder(
    view: View,
    val historyInteractor: HistoryInteractor,
    val selectionHolder: SelectionHolder<History>,
    private val onDeleteClicked: (Int) -> Unit
) : RecyclerView.ViewHolder(view) {

    private lateinit var historyItem: HistoryViewItem.HistoryItem
    val binding = HistoryListHistoryBinding.bind(view)

    init {
        binding.historyLayout.overflowView.apply {
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
     */
    fun bind(item: HistoryViewItem.HistoryItem, mode: HistoryFragmentState.Mode) {
        with(binding.historyLayout) {
            titleView.text = item.data.title
            urlView.text = item.data.url

            setSelectionInteractor(item.data, selectionHolder, historyInteractor)
            changeSelected(item.data in selectionHolder.selectedItems)

            if (!::historyItem.isInitialized ||
                historyItem.data.url != item.data.url
            ) {
                loadFavicon(item.data.url)
            }

            if (mode is HistoryFragmentState.Mode.Editing) {
                overflowView.hideAndDisable()
            } else {
                overflowView.showAndEnable()
            }
        }

        historyItem = item
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_list_history
    }
}

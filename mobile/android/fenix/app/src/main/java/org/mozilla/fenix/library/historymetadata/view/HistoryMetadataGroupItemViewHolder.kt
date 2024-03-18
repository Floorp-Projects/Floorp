/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.view

import android.view.View
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.HistoryMetadataGroupListItemBinding
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.historymetadata.interactor.HistoryMetadataGroupInteractor
import org.mozilla.fenix.selection.SelectionHolder

/**
 * View holder for a history metadata list item.
 */
class HistoryMetadataGroupItemViewHolder(
    view: View,
    private val interactor: HistoryMetadataGroupInteractor,
    private val selectionHolder: SelectionHolder<History.Metadata>,
) : RecyclerView.ViewHolder(view) {

    private val binding = HistoryMetadataGroupListItemBinding.bind(view)

    private var item: History.Metadata? = null

    init {
        binding.historyLayout.overflowView.apply {
            setImageResource(R.drawable.ic_close)
            contentDescription = view.context.getString(R.string.history_delete_item)
            setOnClickListener {
                val item = item ?: return@setOnClickListener
                interactor.onDelete(setOf(item))
            }
        }
    }

    /**
     * Displays the data of the given history record.
     *
     * @param item The [History.Metadata] to display.
     * @param isPendingDeletion Whether or not the [item] is pending deletion.
     */
    fun bind(item: History.Metadata, isPendingDeletion: Boolean) {
        binding.historyLayout.isVisible = !isPendingDeletion
        binding.historyLayout.titleView.text = item.title
        binding.historyLayout.urlView.text = item.url

        binding.historyLayout.setSelectionInteractor(item, selectionHolder, interactor)
        binding.historyLayout.changeSelected(item in selectionHolder.selectedItems)

        if (this.item?.url != item.url) {
            binding.historyLayout.loadFavicon(item.url)
        }

        if (selectionHolder.selectedItems.isEmpty()) {
            binding.historyLayout.overflowView.showAndEnable()
        } else {
            binding.historyLayout.overflowView.hideAndDisable()
        }

        this.item = item
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_metadata_group_list_item
    }
}

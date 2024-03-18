/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.view

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.view.isInvisible
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.SimpleItemAnimator
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentHistoryMetadataGroupBinding
import org.mozilla.fenix.library.LibraryPageView
import org.mozilla.fenix.library.historymetadata.HistoryMetadataGroupFragmentState
import org.mozilla.fenix.library.historymetadata.interactor.HistoryMetadataGroupInteractor

/**
 * Shows a list of history metadata items.
 */
class HistoryMetadataGroupView(
    container: ViewGroup,
    val interactor: HistoryMetadataGroupInteractor,
    val title: String,
    val onEmptyStateChanged: (Boolean) -> Unit,
) : LibraryPageView(container) {

    private val binding = ComponentHistoryMetadataGroupBinding.inflate(
        LayoutInflater.from(container.context),
        container,
        true,
    )

    private val historyMetadataGroupAdapter = HistoryMetadataGroupAdapter(interactor) { isEmpty ->
        onEmptyStateChanged(isEmpty)
    }.apply {
        setHasStableIds(true)
    }
    private var layoutManager: LinearLayoutManager

    init {
        binding.historyMetadataGroupList.apply {
            layoutManager = LinearLayoutManager(containerView.context).also {
                this@HistoryMetadataGroupView.layoutManager = it
            }
            adapter = historyMetadataGroupAdapter
            (itemAnimator as SimpleItemAnimator).supportsChangeAnimations = false
        }
    }

    /**
     * Updates the display of the history metadata items based on the given
     * [HistoryMetadataGroupFragmentState].
     */
    fun update(state: HistoryMetadataGroupFragmentState) {
        binding.historyMetadataGroupList.isInvisible = state.isEmpty
        binding.historyMetadataGroupEmptyView.isVisible = state.isEmpty

        val selectedHistoryItems = state.items.filter {
            it.selected
        }.toSet()

        historyMetadataGroupAdapter.updatePendingDeletionItems(state.pendingDeletionItems)
        historyMetadataGroupAdapter.updateSelectedItems(selectedHistoryItems)
        historyMetadataGroupAdapter.updateData(state.items)

        var first = layoutManager.findFirstVisibleItemPosition()
        val last = layoutManager.findLastVisibleItemPosition()

        // We want to adjust the position of the first visible in order to update the one item above
        // the edge of the screen. It's an edge case, when the item partially visible is being
        // removed. Currently, Undo action won't make it visible again.
        // This block should be above the itemCount calculation, otherwise bottom partially visible
        // item won't be updated.
        if (first > 0) {
            --first
        }

        // In case there are no visible items, we still have to request updating two items, to cover
        // the case when the last item has been removed and Undo action was called.
        val itemCount = if (last != -1) {
            (last - first) + 1
        } else {
            2
        }

        historyMetadataGroupAdapter.notifyItemRangeChanged(first, itemCount)

        if (selectedHistoryItems.isEmpty()) {
            setUiForNormalMode(title)
        } else {
            setUiForSelectingMode(
                context.getString(R.string.history_multi_select_title, selectedHistoryItems.size),
            )
        }
    }
}

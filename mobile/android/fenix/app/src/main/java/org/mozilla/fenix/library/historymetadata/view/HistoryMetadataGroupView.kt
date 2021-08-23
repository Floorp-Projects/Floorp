/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata.view

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
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
    val title: String
) : LibraryPageView(container) {

    private val binding = ComponentHistoryMetadataGroupBinding.inflate(
        LayoutInflater.from(container.context), container, true
    )

    private val historyMetadataGroupAdapter = HistoryMetadataGroupAdapter(interactor)

    init {
        binding.historyMetadataGroupList.apply {
            layoutManager = LinearLayoutManager(containerView.context)
            adapter = historyMetadataGroupAdapter
        }
    }

    /**
     * Updates the display of the history metadata items based on the given
     * [HistoryMetadataGroupFragmentState].
     */
    fun update(state: HistoryMetadataGroupFragmentState) {
        binding.historyMetadataGroupList.isVisible = state.items.isNotEmpty()
        binding.historyMetadataGroupEmptyView.isVisible = state.items.isEmpty()

        historyMetadataGroupAdapter.updateData(state.items)

        val selectedItems = state.items.filter { it.selected }

        if (selectedItems.isEmpty()) {
            setUiForNormalMode(title)
        } else {
            setUiForSelectingMode(
                context.getString(R.string.history_multi_select_title, selectedItems.size)
            )
        }
    }
}

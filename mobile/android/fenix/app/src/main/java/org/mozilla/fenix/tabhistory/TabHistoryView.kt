/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.content.HistoryState
import org.mozilla.fenix.databinding.ComponentTabhistoryBinding

interface TabHistoryViewInteractor {

    /**
     * Jump to a specific index in the tab's history.
     */
    fun goToHistoryItem(item: TabHistoryItem)
}

class TabHistoryView(
    container: ViewGroup,
    private val expandDialog: () -> Unit,
    interactor: TabHistoryViewInteractor
) {

    private val binding = ComponentTabhistoryBinding.inflate(LayoutInflater.from(container.context), container, true)

    private val adapter = TabHistoryAdapter(interactor)
    private val layoutManager = object : LinearLayoutManager(container.context) {

        private var shouldScrollToSelected = true

        override fun onLayoutCompleted(state: RecyclerView.State?) {
            super.onLayoutCompleted(state)
            currentIndex?.let { index ->
                // Attempt to center the current history item after the first layout is completed,
                // but not after subsequent layouts
                if (shouldScrollToSelected) {
                    // Force expansion of the dialog, otherwise scrolling to the current history item
                    // won't work when its position is near the bottom of the recyclerview.
                    expandDialog.invoke()
                    val itemView = binding.tabHistoryRecyclerView.findViewHolderForLayoutPosition(
                        findFirstCompletelyVisibleItemPosition()
                    )?.itemView
                    val offset = binding.tabHistoryRecyclerView.height / 2 - (itemView?.height ?: 0) / 2
                    scrollToPositionWithOffset(index, offset)
                    shouldScrollToSelected = false
                }
            }
        }
    }.apply {
        reverseLayout = true
    }

    private var currentIndex: Int? = null

    init {
        binding.tabHistoryRecyclerView.adapter = adapter
        binding.tabHistoryRecyclerView.layoutManager = layoutManager
    }

    fun updateState(historyState: HistoryState) {
        currentIndex = historyState.currentIndex
        val items = historyState.items.mapIndexed { index, historyItem ->
            TabHistoryItem(
                title = historyItem.title,
                url = historyItem.uri,
                index = index,
                isSelected = index == currentIndex
            )
        }
        adapter.submitList(items)
    }
}

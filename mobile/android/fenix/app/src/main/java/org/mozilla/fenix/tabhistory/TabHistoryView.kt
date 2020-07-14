/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabhistory

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_tabhistory.*
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import org.mozilla.fenix.R

interface TabHistoryViewInteractor {

    /**
     * Jump to a specific index in the tab's history.
     */
    fun goToHistoryItem(item: TabHistoryItem)
}

class TabHistoryView(
    private val container: ViewGroup,
    private val expandDialog: () -> Unit,
    interactor: TabHistoryViewInteractor
) : LayoutContainer {

    override val containerView: View?
        get() = container

    val view: View = LayoutInflater.from(container.context)
        .inflate(R.layout.component_tabhistory, container, true)

    private val adapter = TabHistoryAdapter(interactor)
    private val layoutManager = object : LinearLayoutManager(view.context) {
        override fun onLayoutCompleted(state: RecyclerView.State?) {
            super.onLayoutCompleted(state)
            currentIndex?.let { index ->
                // Force expansion of the dialog, otherwise scrolling to the current history item
                // won't work when its position is near the bottom of the recyclerview.
                expandDialog.invoke()
                // Also, attempt to center the current history item.
                val itemView = tabHistoryRecyclerView.findViewHolderForLayoutPosition(
                    findFirstCompletelyVisibleItemPosition()
                )?.itemView
                val offset = tabHistoryRecyclerView.height / 2 - (itemView?.height ?: 0) / 2
                scrollToPositionWithOffset(index, offset)
            }
        }
    }.apply {
        reverseLayout = true
    }

    private var currentIndex: Int? = null

    init {
        tabHistoryRecyclerView.adapter = adapter
        tabHistoryRecyclerView.layoutManager = layoutManager
    }

    fun updateState(state: BrowserState) {
        state.selectedTab?.content?.history?.let { historyState ->
            currentIndex = historyState.currentIndex
            val items = historyState.items.mapIndexed { index, historyItem ->
                TabHistoryItem(
                    title = historyItem.title,
                    url = historyItem.uri,
                    index = index,
                    isSelected = index == historyState.currentIndex
                )
            }
            adapter.historyList = items
        }
    }
}

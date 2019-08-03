/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.SimpleItemAnimator
import kotlinx.android.synthetic.main.component_history.*
import kotlinx.android.synthetic.main.component_history.view.*
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibraryPageView
import org.mozilla.fenix.library.SelectionInteractor

/**
 * Interface for the HistoryViewInteractor. This interface is implemented by objects that want
 * to respond to user interaction on the HistoryView
 */
interface HistoryViewInteractor : SelectionInteractor<HistoryItem> {

    /**
     * Called on backpressed to exit edit mode
     */
    fun onBackPressed(): Boolean

    /**
     * Called when the mode is switched so we can invalidate the menu
     */
    fun onModeSwitched()

    /**
     * Called when delete all is tapped
     */
    fun onDeleteAll()

    /**
     * Called when multiple history items are deleted
     * @param items the history items to delete
     */
    fun onDeleteSome(items: Set<HistoryItem>)
}

/**
 * View that contains and configures the History List
 */
class HistoryView(
    container: ViewGroup,
    val interactor: HistoryInteractor
) : LibraryPageView(container), BackHandler {

    val view: View = LayoutInflater.from(container.context)
        .inflate(R.layout.component_history, container, true)

    private var items: List<HistoryItem> = listOf()
    var mode: HistoryState.Mode = HistoryState.Mode.Normal
        private set

    val historyAdapter = HistoryAdapter(interactor)
    private val layoutManager = LinearLayoutManager(container.context)

    init {
        view.history_list.apply {
            layoutManager = this@HistoryView.layoutManager
            adapter = historyAdapter
            (itemAnimator as SimpleItemAnimator).supportsChangeAnimations = false
        }
    }

    fun update(state: HistoryState) {
        val oldMode = mode

        view.progress_bar.isVisible = state.mode === HistoryState.Mode.Deleting
        items = state.items
        mode = state.mode

        if (state.mode != oldMode) {
            interactor.onModeSwitched()
            historyAdapter.updateMode(state.mode)

            // Deselect all the previously selected items
            oldMode.selectedItems.forEach {
                historyAdapter.notifyItemChanged(it.id)
            }
        }

        if (state.mode is HistoryState.Mode.Editing) {
            val unselectedItems = oldMode.selectedItems - state.mode.selectedItems

            state.mode.selectedItems.union(unselectedItems).forEach { item ->
                historyAdapter.notifyItemChanged(item.id)
            }
        }

        when (val mode = state.mode) {
            is HistoryState.Mode.Normal ->
                setUiForNormalMode(context.getString(R.string.library_history))
            is HistoryState.Mode.Editing ->
                setUiForSelectingMode(context.getString(R.string.history_multi_select_title, mode.selectedItems.size))
        }
    }

    fun updateEmptyState(userHasHistory: Boolean) {
        history_list.isVisible = userHasHistory
        history_empty_view.isVisible = !userHasHistory
    }

    override fun onBackPressed(): Boolean {
        return interactor.onBackPressed()
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.widget.NestedScrollView
import androidx.recyclerview.widget.LinearLayoutManager
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_history.view.*
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView

class HistoryUIView(
    container: ViewGroup,
    actionEmitter: Observer<HistoryAction>,
    changesObservable: Observable<HistoryChange>
) :
    UIView<HistoryState, HistoryAction, HistoryChange>(container, actionEmitter, changesObservable),
    BackHandler {

    var mode: HistoryState.Mode = HistoryState.Mode.Normal
        private set

    override val view: NestedScrollView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_history, container, true)
        .findViewById(R.id.history_wrapper)

    init {
        view.history_list.apply {
            adapter = HistoryAdapter(actionEmitter)
            layoutManager = LinearLayoutManager(container.context)
        }

        view.delete_history_button.setOnClickListener {
            val mode = mode
            val action = when (mode) {
                is HistoryState.Mode.Normal -> HistoryAction.Delete.All
                is HistoryState.Mode.Editing -> HistoryAction.Delete.Some(mode.selectedItems)
            }

            actionEmitter.onNext(action)
        }
    }

    override fun updateView() = Consumer<HistoryState> {
        mode = it.mode
        updateDeleteButton()
        (view.history_list.adapter as HistoryAdapter).updateData(it.items, it.mode)
    }

    private fun updateDeleteButton() {
        val mode = mode

        val text = if (mode is HistoryState.Mode.Editing && mode.selectedItems.isNotEmpty()) {
            view.delete_history_button_text.context.resources.getString(
                R.string.history_delete_some,
                mode.selectedItems.size
            )
        } else {
            view.delete_history_button_text.context.resources.getString(R.string.history_delete_all)
        }

        view.delete_history_button.contentDescription = text
        view.delete_history_button_text.text = text
    }
    override fun onBackPressed(): Boolean {
        if (mode is HistoryState.Mode.Editing) {
            actionEmitter.onNext(HistoryAction.BackPressed)
            return true
        }

        return false
    }
}

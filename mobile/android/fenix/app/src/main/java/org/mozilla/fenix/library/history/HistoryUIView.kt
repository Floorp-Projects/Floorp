/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
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

    override val view: RecyclerView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_history, container, true)
        .findViewById(R.id.history_list)

    init {
        view.apply {
            adapter = HistoryAdapter(actionEmitter)
            layoutManager = LinearLayoutManager(container.context)
        }
    }

    override fun updateView() = Consumer<HistoryState> {
        mode = it.mode
        (view.adapter as HistoryAdapter).updateData(it.items, it.mode)
    }

    override fun onBackPressed(): Boolean {
        if (mode is HistoryState.Mode.Editing) {
            actionEmitter.onNext(HistoryAction.BackPressed)
            return true
        }

        return false
    }
}

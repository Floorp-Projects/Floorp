/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_history.*
import kotlinx.android.synthetic.main.component_history.view.*
import kotlinx.android.synthetic.main.delete_history_button.*
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getColorIntFromAttr
import org.mozilla.fenix.library.LibraryPageUIView

class HistoryUIView(
    container: ViewGroup,
    actionEmitter: Observer<HistoryAction>,
    changesObservable: Observable<HistoryChange>
) :
    LibraryPageUIView<HistoryState, HistoryAction, HistoryChange>(container, actionEmitter, changesObservable),
    BackHandler {

    var mode: HistoryState.Mode = HistoryState.Mode.Normal
        private set

    private val historyAdapter: HistoryAdapter
    private var items: List<HistoryItem> = listOf()

    fun getSelected(): List<HistoryItem> = historyAdapter.selected

    override val view: ConstraintLayout = LayoutInflater.from(container.context)
        .inflate(R.layout.component_history, container, true)
        .findViewById(R.id.history_wrapper)

    init {
        view.history_list.apply {
            historyAdapter = HistoryAdapter(actionEmitter)
            adapter = historyAdapter
            layoutManager = LinearLayoutManager(container.context)
        }
    }

    override fun updateView() = Consumer<HistoryState> {
        view.progress_bar.visibility = if (it.mode is HistoryState.Mode.Deleting) View.VISIBLE else View.GONE

        if (it.mode != mode) {
            mode = it.mode
            actionEmitter.onNext(HistoryAction.SwitchMode)
        }

        (view.history_list.adapter as HistoryAdapter).updateData(it.items, it.mode)

        items = it.items
        when (val modeCopy = mode) {
            is HistoryState.Mode.Normal -> setUIForNormalMode(items.isEmpty())
            is HistoryState.Mode.Editing -> setUIForSelectingMode(modeCopy)
        }
    }

    private fun setUIForSelectingMode(
        mode: HistoryState.Mode.Editing
    ) {
        activity?.title =
                context.getString(R.string.history_multi_select_title, mode.selectedItems.size)
        setToolbarColors(
            R.color.white_color,
            R.attr.accentHighContrast.getColorIntFromAttr(context!!)
        )
    }

    private fun setUIForNormalMode(isEmpty: Boolean) {
        activity?.title = context.getString(R.string.library_history)
        delete_history_button?.isVisible = !isEmpty
        history_empty_view.isVisible = isEmpty
        setToolbarColors(
            R.attr.primaryText.getColorIntFromAttr(context!!),
            R.attr.foundation.getColorIntFromAttr(context)
        )
    }

    override fun onBackPressed(): Boolean {
        return when (mode) {
            is HistoryState.Mode.Editing -> {
                mode = HistoryState.Mode.Normal
                historyAdapter.updateData(items, mode)
                setUIForNormalMode(items.isEmpty())
                actionEmitter.onNext(HistoryAction.BackPressed)
                true
            }
            else -> false
        }
    }
}

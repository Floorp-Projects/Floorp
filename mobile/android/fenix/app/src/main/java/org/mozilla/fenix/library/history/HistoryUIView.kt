/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.graphics.PorterDuff.Mode.SRC_IN
import android.graphics.PorterDuffColorFilter
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_history.*
import kotlinx.android.synthetic.main.component_history.view.*
import kotlinx.android.synthetic.main.delete_history_button.*
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.getColorIntFromAttr
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

    private val historyAdapter: HistoryAdapter
    private var items: List<HistoryItem> = listOf()
    private val context = container.context
    private val activity = context?.asActivity()

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
        (activity as? AppCompatActivity)?.title =
                context.getString(R.string.history_multi_select_title, mode.selectedItems.size)
        setToolbarColors(
            R.color.white_color,
            R.attr.accentHighContrast.getColorIntFromAttr(context!!)
        )
    }

    private fun setUIForNormalMode(isEmpty: Boolean) {
        (activity as? AppCompatActivity)?.title = context.getString(R.string.library_history)
        delete_history_button?.visibility = if (isEmpty) View.GONE else View.VISIBLE
        history_empty_view.visibility = if (isEmpty) View.VISIBLE else View.GONE
        setToolbarColors(
            R.attr.primaryText.getColorIntFromAttr(context!!),
            R.attr.foundation.getColorIntFromAttr(context)
        )
    }

    private fun setToolbarColors(foreground: Int, background: Int) {
        val toolbar = (activity as AppCompatActivity).findViewById<Toolbar>(R.id.navigationToolbar)
        val colorFilter = PorterDuffColorFilter(ContextCompat.getColor(context, foreground), SRC_IN)
        toolbar.setBackgroundColor(ContextCompat.getColor(context, background))
        toolbar.setTitleTextColor(ContextCompat.getColor(context, foreground))

        themeToolbar(
            toolbar, foreground,
            background, colorFilter
        )
    }

    private fun themeToolbar(
        toolbar: androidx.appcompat.widget.Toolbar,
        textColor: Int,
        backgroundColor: Int,
        colorFilter: PorterDuffColorFilter? = null
    ) {
        toolbar.setTitleTextColor(ContextCompat.getColor(context!!, textColor))
        toolbar.setBackgroundColor(ContextCompat.getColor(context, backgroundColor))

        if (colorFilter == null) {
            return
        }

        toolbar.overflowIcon?.colorFilter = colorFilter
        (0 until toolbar.childCount).forEach {
            when (val item = toolbar.getChildAt(it)) {
                is ImageButton -> item.drawable.colorFilter = colorFilter
            }
        }
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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.SimpleItemAnimator
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_history.*
import kotlinx.android.synthetic.main.component_history.view.*
import kotlinx.android.synthetic.main.component_history.view.history_list
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.getColorResFromAttr

/**
 * Interface for the HistoryViewInteractor. This interface is implemented by objects that want
 * to respond to user interaction on the HistoryView
 */
interface HistoryViewInteractor {
    /**
     * Called when a user taps a history item
     */
    fun onItemPress(item: HistoryItem)

    /**
     * Called when a user long clicks a user
     */
    fun onItemLongPress(item: HistoryItem)

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
     * Called when one history item is deleted
     * @param item the history item to delete
     */
    fun onDeleteOne(item: HistoryItem)

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
    private val container: ViewGroup,
    val interactor: HistoryInteractor
) : LayoutContainer, BackHandler {

    val view: ConstraintLayout = LayoutInflater.from(container.context)
        .inflate(R.layout.component_history, container, true)
        .findViewById(R.id.history_wrapper)

    override val containerView: View?
        get() = container

    val historyAdapter: HistoryAdapter
    private var items: List<HistoryItem> = listOf()
    private val context = container.context
    var mode: HistoryState.Mode = HistoryState.Mode.Normal
        private set
    private val activity = context?.asActivity()
    private val layoutManager = LinearLayoutManager(container.context)
    init {
        historyAdapter = HistoryAdapter(interactor)

        view.history_list.apply {
            layoutManager = this@HistoryView.layoutManager
            adapter = historyAdapter
            (itemAnimator as SimpleItemAnimator).supportsChangeAnimations = false
        }
    }

    fun update(state: HistoryState) {
        view.progress_bar.visibility =
            if (state.mode is HistoryState.Mode.Deleting) View.VISIBLE else View.GONE

        if (state.mode != mode) {
            interactor.onModeSwitched()
            historyAdapter.updateMode(state.mode)

            val oldMode = mode
            if (oldMode is HistoryState.Mode.Editing) {
                oldMode.selectedItems.forEach {
                    historyAdapter.notifyItemChanged(it.id)
                }
            }
        }

        (state.mode as? HistoryState.Mode.Editing)?.also {
            val oldMode = (mode as? HistoryState.Mode.Editing)
            val unselectedItems = oldMode?.selectedItems?.minus(it.selectedItems) ?: setOf()

            it.selectedItems.union(unselectedItems).forEach { item ->
                historyAdapter.notifyItemChanged(item.id)
            }
        }

        items = state.items
        when (val mode = state.mode) {
            is HistoryState.Mode.Normal -> setUIForNormalMode()
            is HistoryState.Mode.Editing -> setUIForSelectingMode(mode.selectedItems.size)
        }

        mode = state.mode
    }

    fun updateEmptyState(userHasHistory: Boolean) {
        history_list.isVisible = userHasHistory
        history_empty_view.isVisible = !userHasHistory
    }

    private fun setUIForSelectingMode(selectedItemSize: Int) {
        activity?.title =
            context.getString(R.string.history_multi_select_title, selectedItemSize)
        setToolbarColors(
            R.color.white_color,
            context!!.getColorResFromAttr(R.attr.accentHighContrast)
        )
    }

    private fun setUIForNormalMode() {
        activity?.title = context.getString(R.string.library_history)
        setToolbarColors(
            context!!.getColorResFromAttr(R.attr.primaryText),
            context.getColorResFromAttr(R.attr.foundation)
        )
    }

    private fun setToolbarColors(foreground: Int, background: Int) {
        val toolbar = (activity as AppCompatActivity).findViewById<Toolbar>(R.id.navigationToolbar)
        val colorFilter = PorterDuffColorFilter(
            ContextCompat.getColor(context, foreground),
            PorterDuff.Mode.SRC_IN
        )
        toolbar.setBackgroundColor(ContextCompat.getColor(context, background))
        toolbar.setTitleTextColor(ContextCompat.getColor(context, foreground))

        themeToolbar(
            toolbar, foreground,
            background, colorFilter
        )
    }

    private fun themeToolbar(
        toolbar: Toolbar,
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
        return interactor.onBackPressed()
    }
}

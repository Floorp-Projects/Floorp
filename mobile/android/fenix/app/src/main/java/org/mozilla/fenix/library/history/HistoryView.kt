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
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_history.*
import kotlinx.android.synthetic.main.component_history.view.*
import kotlinx.android.synthetic.main.delete_history_button.*
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.getColorIntFromAttr

/**
 * Interface for the HistoryViewInteractor. This interface is implemented by objects that want
 * to respond to user interaction on the HistoryView
 */
interface HistoryViewInteractor {
    /**
     * Called whenever a history item is tapped to open that history entry in the browser
     * @param item the history item to open in browser
     */
    fun onHistoryItemOpened(item: HistoryItem)

    /**
     * Called when a history item is long pressed and edit mode is launched
     * @param selectedItem the history item to start selected for deletion in edit mode
     */
    fun onEnterEditMode(selectedItem: HistoryItem)

    /**
     * Called on backpressed to exit edit mode
     */
    fun onBackPressed()

    /**
     * Called when a history item is tapped in edit mode and added for removal
     * @param item the history item to add to selected items for deletion in edit mode
     */
    fun onItemAddedForRemoval(item: HistoryItem)

    /**
     * Called when a selected history item is tapped in edit mode and removed from removal
     * @param item the history item to remove from the selected items for deletion in edit mode
     */
    fun onItemRemovedForRemoval(item: HistoryItem)

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
    fun onDeleteSome(items: List<HistoryItem>)
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

    private val historyAdapter: HistoryAdapter
    private var items: List<HistoryItem> = listOf()
    private val context = container.context
    var mode: HistoryState.Mode = HistoryState.Mode.Normal
        private set
    private val activity = context?.asActivity()

    init {
        view.history_list.apply {
            historyAdapter = HistoryAdapter(interactor)
            adapter = historyAdapter
            layoutManager = LinearLayoutManager(container.context)
        }
    }

    fun update(state: HistoryState) {
        view.progress_bar.visibility =
            if (state.mode is HistoryState.Mode.Deleting) View.VISIBLE else View.GONE

        if (state.mode != mode) {
            mode = state.mode
            interactor.onModeSwitched()
        }

        (view.history_list.adapter as HistoryAdapter).updateData(state.items, state.mode)

        items = state.items
        when (val mode = mode) {
            is HistoryState.Mode.Normal -> setUIForNormalMode(items.isEmpty())
            is HistoryState.Mode.Editing -> setUIForSelectingMode(mode.selectedItems.size)
        }
    }

    private fun setUIForSelectingMode(selectedItemSize: Int) {
        activity?.title =
            context.getString(R.string.history_multi_select_title, selectedItemSize)
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
        return when (mode) {
            is HistoryState.Mode.Editing -> {
                mode = HistoryState.Mode.Normal
                historyAdapter.updateData(items, mode)
                setUIForNormalMode(items.isEmpty())
                interactor.onBackPressed()
                true
            }
            else -> false
        }
    }
}

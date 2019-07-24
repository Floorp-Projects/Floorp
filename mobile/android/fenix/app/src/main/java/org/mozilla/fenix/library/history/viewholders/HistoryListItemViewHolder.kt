/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.history_list_item.view.*
import mozilla.components.browser.menu.BrowserMenu
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.library.history.HistoryInteractor
import org.mozilla.fenix.library.history.HistoryItem
import org.mozilla.fenix.library.history.HistoryItemMenu
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
import org.mozilla.fenix.library.history.HistoryState

class HistoryListItemViewHolder(
    private val view: View,
    private val historyInteractor: HistoryInteractor
) : RecyclerView.ViewHolder(view) {

    private val layout = view.history_layout
    private val favicon = view.history_favicon
    private val title = view.history_title
    private val url = view.history_url
    private val menuButton = view.history_item_overflow
    private val headerWrapper = view.header_wrapper
    private val headerTitle = view.header_title
    private val deleteButtonWrapper = view.delete_button_wrapper
    private val deleteButton = view.delete_button

    private var item: HistoryItem? = null
    private lateinit var historyMenu: HistoryItemMenu
    private var mode: HistoryState.Mode = HistoryState.Mode.Normal

    init {
        setupMenu()

        layout.setOnLongClickListener {
            item?.apply {
                historyInteractor.onItemLongPress(this)
            }

            true
        }

        menuButton.setOnClickListener {
            historyMenu.menuBuilder.build(view.context).show(
                anchor = it,
                orientation = BrowserMenu.Orientation.DOWN
            )
        }

        itemView.history_layout.setOnClickListener {
            item?.also(historyInteractor::onItemPress)
        }

        deleteButton.setOnClickListener {
            mode?.also {
                when (it) {
                    is HistoryState.Mode.Normal -> historyInteractor.onDeleteAll()
                    is HistoryState.Mode.Editing -> historyInteractor.onDeleteSome(it.selectedItems)
                }
            }
        }
    }

    fun bind(
        item: HistoryItem,
        timeGroup: HistoryItemTimeGroup?,
        showDeletebutton: Boolean,
        mode: HistoryState.Mode
    ) {
        this.item = item
        this.mode = mode

        title.text = item.title
        url.text = item.url

        toggleDeleteButton(showDeletebutton, mode)

        val headerText = timeGroup?.let { it.humanReadable(view.context) }
        toggleHeader(headerText)

        val selected = toggleSelected(mode, item)

        if (mode is HistoryState.Mode.Editing) {
            val backgroundTint =
                if (selected) {
                    ThemeManager.resolveAttribute(R.attr.accentHighContrast, itemView.context)
                } else {
                    ThemeManager.resolveAttribute(R.attr.neutral, itemView.context)
                }
            val backgroundTintList =
                ContextCompat.getColorStateList(itemView.context, backgroundTint)
            favicon.backgroundTintList = backgroundTintList

            if (selected) {
                favicon.setImageResource(R.drawable.mozac_ic_check)
            } else {
                updateFavIcon(item.url)
            }
        } else {
            val backgroundTint = ThemeManager.resolveAttribute(R.attr.neutral, itemView.context)
            val backgroundTintList =
                ContextCompat.getColorStateList(itemView.context, backgroundTint)
            favicon.backgroundTintList = backgroundTintList
            updateFavIcon(item.url)
        }
    }

    private fun toggleSelected(
        mode: HistoryState.Mode,
        item: HistoryItem
    ): Boolean {
        return when (mode) {
            is HistoryState.Mode.Editing -> mode.selectedItems.contains(item)
            else -> false
        }
    }

    private fun toggleHeader(text: String?) {
        text?.also {
            headerWrapper.visibility = View.VISIBLE
            headerTitle.text = it
        } ?: run {
            headerWrapper.visibility = View.GONE
        }
    }

    private fun toggleDeleteButton(
        showDeletebutton: Boolean,
        mode: HistoryState.Mode
    ) {
        if (showDeletebutton) {
            deleteButtonWrapper.visibility = View.VISIBLE

            deleteButton.run {
                val isDeleting = mode is HistoryState.Mode.Deleting
                if (isDeleting || mode is HistoryState.Mode.Editing && mode.selectedItems.isNotEmpty()) {
                    isEnabled = false
                    alpha = DELETE_BUTTON_DISABLED_ALPHA
                } else {
                    isEnabled = true
                    alpha = 1f
                }
            }
        } else {
            deleteButtonWrapper.visibility = View.GONE
        }
    }

    private fun setupMenu() {
        this.historyMenu = HistoryItemMenu(itemView.context) {
            when (it) {
                is HistoryItemMenu.Item.Delete -> {
                    item?.apply { historyInteractor.onDeleteOne(this) }
                }
            }
        }
    }

    private fun updateFavIcon(url: String) {
        favicon.context.components.core.icons.loadIntoView(favicon, url)
    }

    companion object {
        const val DELETE_BUTTON_DISABLED_ALPHA = 0.4f
        const val LAYOUT_ID = R.layout.history_list_item
    }
}

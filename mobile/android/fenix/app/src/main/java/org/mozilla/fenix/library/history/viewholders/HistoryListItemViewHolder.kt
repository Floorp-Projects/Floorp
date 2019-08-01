/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.history_list_item.view.*
import mozilla.components.browser.menu.BrowserMenu
import org.mozilla.fenix.R
import org.mozilla.fenix.library.history.HistoryInteractor
import org.mozilla.fenix.library.history.HistoryItem
import org.mozilla.fenix.library.history.HistoryItemMenu
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
import org.mozilla.fenix.library.history.HistoryState

class HistoryListItemViewHolder(
    view: View,
    private val historyInteractor: HistoryInteractor
) : RecyclerView.ViewHolder(view) {

    private var item: HistoryItem? = null
    private var mode: HistoryState.Mode = HistoryState.Mode.Normal

    init {
        setupMenu()

        itemView.history_layout.setOnLongClickListener {
            item?.also(historyInteractor::onItemLongPress)
            true
        }

        itemView.history_layout.setOnClickListener {
            item?.also(historyInteractor::onItemPress)
        }

        itemView.delete_button.setOnClickListener {
            when (val mode = this.mode) {
                HistoryState.Mode.Normal -> historyInteractor.onDeleteAll()
                is HistoryState.Mode.Editing -> historyInteractor.onDeleteSome(mode.selectedItems)
            }
        }
    }

    fun bind(
        item: HistoryItem,
        timeGroup: HistoryItemTimeGroup?,
        showDeleteButton: Boolean,
        mode: HistoryState.Mode
    ) {
        this.item = item
        this.mode = mode

        itemView.history_layout.titleView.text = item.title
        itemView.history_layout.urlView.text = item.url

        toggleDeleteButton(showDeleteButton, mode)

        val headerText = timeGroup?.humanReadable(itemView.context)
        toggleHeader(headerText)

        itemView.history_layout.changeSelected(item in mode.selectedItems)
        itemView.history_layout.loadFavicon(item.url)
    }

    private fun toggleHeader(text: String?) {
        if (text != null) {
            itemView.header_title.visibility = View.VISIBLE
            itemView.header_title.text = text
        } else {
            itemView.header_title.visibility = View.GONE
        }
    }

    private fun toggleDeleteButton(
        showDeleteButton: Boolean,
        mode: HistoryState.Mode
    ) {
        if (showDeleteButton) {
            itemView.delete_button.run {
                visibility = View.VISIBLE

                if (mode === HistoryState.Mode.Deleting || mode.selectedItems.isNotEmpty()) {
                    isEnabled = false
                    alpha = DELETE_BUTTON_DISABLED_ALPHA
                } else {
                    isEnabled = true
                    alpha = 1f
                }
            }
        } else {
            itemView.delete_button.visibility = View.GONE
        }
    }

    private fun setupMenu() {
        val historyMenu = HistoryItemMenu(itemView.context) {
            when (it) {
                HistoryItemMenu.Item.Delete -> item?.also(historyInteractor::onDeleteOne)
            }
        }

        itemView.history_layout.overflowView.setOnClickListener {
            historyMenu.menuBuilder.build(itemView.context).show(
                anchor = it,
                orientation = BrowserMenu.Orientation.DOWN
            )
        }
    }

    companion object {
        const val DELETE_BUTTON_DISABLED_ALPHA = 0.4f
        const val LAYOUT_ID = R.layout.history_list_item
    }
}

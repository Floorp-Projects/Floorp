/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.history_list_item.view.*
import kotlinx.android.synthetic.main.library_site_item.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.SelectionHolder
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryInteractor
import org.mozilla.fenix.library.history.HistoryItem
import org.mozilla.fenix.library.history.HistoryItemMenu
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
import org.mozilla.fenix.utils.Do

class HistoryListItemViewHolder(
    view: View,
    private val historyInteractor: HistoryInteractor,
    private val selectionHolder: SelectionHolder<HistoryItem>
) : RecyclerView.ViewHolder(view) {

    private var item: HistoryItem? = null

    init {
        setupMenu()

        itemView.delete_button.setOnClickListener {
            val selected = selectionHolder.selectedItems
            if (selected.isEmpty()) {
                historyInteractor.onDeleteAll()
            } else {
                historyInteractor.onDeleteSome(selected)
            }
        }
    }

    fun bind(
        item: HistoryItem,
        timeGroup: HistoryItemTimeGroup?,
        showDeleteButton: Boolean,
        mode: HistoryFragmentState.Mode,
        isPendingDeletion: Boolean = false
    ) {
        if (isPendingDeletion) {
            itemView.history_layout.visibility = View.GONE
        } else {
            itemView.history_layout.visibility = View.VISIBLE
        }

        itemView.history_layout.titleView.text = item.title
        itemView.history_layout.urlView.text = item.url

        toggleDeleteButton(showDeleteButton, mode === HistoryFragmentState.Mode.Normal)

        val headerText = timeGroup?.humanReadable(itemView.context)
        toggleHeader(headerText)

        itemView.history_layout.setSelectionInteractor(item, selectionHolder, historyInteractor)
        itemView.history_layout.changeSelected(item in selectionHolder.selectedItems)

        if (this.item?.url != item.url) {
            itemView.history_layout.loadFavicon(item.url)
        }

        if (mode is HistoryFragmentState.Mode.Editing) {
            itemView.overflow_menu.hideAndDisable()
        } else {
            itemView.overflow_menu.showAndEnable()
        }

        this.item = item
    }

    private fun toggleHeader(headerText: String?) {
        if (headerText != null) {
            itemView.header_title.visibility = View.VISIBLE
            itemView.header_title.text = headerText
        } else {
            itemView.header_title.visibility = View.GONE
        }
    }

    private fun toggleDeleteButton(
        showDeleteButton: Boolean,
        isNormalMode: Boolean
    ) {
        if (showDeleteButton) {
            itemView.delete_button.run {
                visibility = View.VISIBLE

                if (isNormalMode) {
                    isEnabled = true
                    alpha = 1f
                } else {
                    isEnabled = false
                    alpha = DELETE_BUTTON_DISABLED_ALPHA
                }
            }
        } else {
            itemView.delete_button.visibility = View.GONE
        }
    }

    private fun setupMenu() {
        val historyMenu = HistoryItemMenu(itemView.context) {
            val item = this.item ?: return@HistoryItemMenu
            Do exhaustive when (it) {
                HistoryItemMenu.Item.Copy -> historyInteractor.onCopyPressed(item)
                HistoryItemMenu.Item.Share -> historyInteractor.onSharePressed(item)
                HistoryItemMenu.Item.OpenInNewTab -> historyInteractor.onOpenInNormalTab(item)
                HistoryItemMenu.Item.OpenInPrivateTab -> historyInteractor.onOpenInPrivateTab(item)
                HistoryItemMenu.Item.Delete -> historyInteractor.onDeleteSome(setOf(item))
            }
        }

        itemView.history_layout.attachMenu(historyMenu.menuController)
    }

    companion object {
        const val DELETE_BUTTON_DISABLED_ALPHA = 0.4f
        const val LAYOUT_ID = R.layout.history_list_item
    }
}

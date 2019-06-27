/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.menu.BrowserMenu
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.history.HistoryInteractor
import org.mozilla.fenix.library.history.HistoryItem
import org.mozilla.fenix.library.history.HistoryItemMenu
import org.mozilla.fenix.library.history.HistoryState

class HistoryListItemViewHolder(
    val view: LibrarySiteItemView,
    private val historyInteractor: HistoryInteractor
) : RecyclerView.ViewHolder(view) {

    private var item: HistoryItem? = null
    private lateinit var historyMenu: HistoryItemMenu
    private var mode: HistoryState.Mode = HistoryState.Mode.Normal

    init {
        setupMenu()

        view.setOnLongClickListener {
            item?.apply {
                historyInteractor.onEnterEditMode(this)
            }

            true
        }

        view.overflowView.setOnClickListener {
            historyMenu.menuBuilder.build(view.context).show(
                anchor = it,
                orientation = BrowserMenu.Orientation.DOWN
            )
        }

        view.displayAs(LibrarySiteItemView.ItemType.SITE)
    }

    fun bind(item: HistoryItem, mode: HistoryState.Mode) {
        this.item = item
        this.mode = mode

        view.titleView.text = item.title
        view.urlView.text = item.url

        val selected = mode is HistoryState.Mode.Editing && mode.selectedItems.contains(item)

        setClickListeners(item, selected)

        view.changeSelected(selected)
        view.loadFavicon(item.url)
    }

    private fun setupMenu() {
        historyMenu = HistoryItemMenu(view.context) {
            when (it) {
                is HistoryItemMenu.Item.Delete -> {
                    item?.apply { historyInteractor.onDeleteOne(this) }
                }
            }
        }
    }

    private fun setClickListeners(
        item: HistoryItem,
        selected: Boolean
    ) {
        view.setOnClickListener {
            when {
                mode == HistoryState.Mode.Normal -> historyInteractor.onHistoryItemOpened(item)
                selected -> historyInteractor.onItemRemovedForRemoval(item)
                else -> historyInteractor.onItemAddedForRemoval(item)
            }
        }
    }

    companion object {
        val ID = LibrarySiteItemView.ItemType.SITE.ordinal
    }
}

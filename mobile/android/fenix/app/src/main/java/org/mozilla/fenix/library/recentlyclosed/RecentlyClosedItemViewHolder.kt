/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.history_list_item.view.*
import kotlinx.android.synthetic.main.library_site_item.view.*
import mozilla.components.browser.state.state.recover.RecoverableTab
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.library.history.HistoryItemMenu
import org.mozilla.fenix.utils.Do

class RecentlyClosedItemViewHolder(
    view: View,
    private val recentlyClosedFragmentInteractor: RecentlyClosedFragmentInteractor,
    private val selectionHolder: SelectionHolder<RecoverableTab>
) : RecyclerView.ViewHolder(view) {

    private var item: RecoverableTab? = null

    init {
        setupMenu()
    }

    fun bind(
        item: RecoverableTab
    ) {
        itemView.history_layout.titleView.text =
            if (item.title.isNotEmpty()) item.title else item.url
        itemView.history_layout.urlView.text = item.url

        itemView.history_layout.setSelectionInteractor(item, selectionHolder, recentlyClosedFragmentInteractor)
        itemView.history_layout.changeSelected(item in selectionHolder.selectedItems)

        if (this.item?.url != item.url) {
            itemView.history_layout.loadFavicon(item.url)
        }

        if (selectionHolder.selectedItems.isEmpty()) {
            itemView.overflow_menu.showAndEnable()
        } else {
            itemView.overflow_menu.hideAndDisable()
        }

        this.item = item
    }

    private fun setupMenu() {
        val historyMenu = HistoryItemMenu(itemView.context) {
            val item = this.item ?: return@HistoryItemMenu
            Do exhaustive when (it) {
                HistoryItemMenu.Item.Copy -> recentlyClosedFragmentInteractor.onCopyPressed(item)
                HistoryItemMenu.Item.Share -> recentlyClosedFragmentInteractor.onSharePressed(item)
                HistoryItemMenu.Item.OpenInNewTab -> recentlyClosedFragmentInteractor.onOpenInNormalTab(
                    item
                )
                HistoryItemMenu.Item.OpenInPrivateTab -> recentlyClosedFragmentInteractor.onOpenInPrivateTab(
                    item
                )
                HistoryItemMenu.Item.Delete -> recentlyClosedFragmentInteractor.onDelete(item)
            }
        }

        itemView.history_layout.attachMenu(historyMenu.menuController)
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_list_item
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.viewholders

import android.view.View
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.HistoryListItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.selection.SelectionHolder
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
    private val binding = HistoryListItemBinding.bind(view)

    init {
        setupMenu()

        binding.deleteButton.setOnClickListener {
            val selected = selectionHolder.selectedItems
            if (selected.isEmpty()) {
                historyInteractor.onDeleteAll()
            } else {
                historyInteractor.onDeleteSome(selected)
            }
        }

        binding.recentlyClosedNavEmpty.recentlyClosedNav.setOnClickListener {
            historyInteractor.onRecentlyClosedClicked()
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
            binding.historyLayout.visibility = View.GONE
        } else {
            binding.historyLayout.visibility = View.VISIBLE
        }

        binding.historyLayout.titleView.text = item.title
        binding.historyLayout.urlView.text = item.url

        toggleTopContent(showDeleteButton, mode === HistoryFragmentState.Mode.Normal)

        val headerText = timeGroup?.humanReadable(itemView.context)
        toggleHeader(headerText)

        binding.historyLayout.setSelectionInteractor(item, selectionHolder, historyInteractor)
        binding.historyLayout.changeSelected(item in selectionHolder.selectedItems)

        if (this.item?.url != item.url) {
            binding.historyLayout.loadFavicon(item.url)
        }

        if (mode is HistoryFragmentState.Mode.Editing) {
            binding.historyLayout.overflowView.hideAndDisable()
        } else {
            binding.historyLayout.overflowView.showAndEnable()
        }

        this.item = item
    }

    private fun toggleHeader(headerText: String?) {
        if (headerText != null) {
            binding.headerTitle.visibility = View.VISIBLE
            binding.headerTitle.text = headerText
        } else {
            binding.headerTitle.visibility = View.GONE
        }
    }

    private fun toggleTopContent(
        showTopContent: Boolean,
        isNormalMode: Boolean
    ) {
        binding.deleteButton.isVisible = showTopContent
        binding.recentlyClosedNavEmpty.recentlyClosedNav.isVisible = showTopContent

        if (showTopContent) {
            binding.deleteButton.run {
                if (isNormalMode) {
                    isEnabled = true
                    alpha = 1f
                } else {
                    isEnabled = false
                    alpha = DELETE_BUTTON_DISABLED_ALPHA
                }
            }
            val numRecentTabs = itemView.context.components.core.store.state.closedTabs.size
            binding.recentlyClosedNavEmpty.recentlyClosedTabsDescription.text = String.format(
                itemView.context.getString(
                    if (numRecentTabs == 1)
                        R.string.recently_closed_tab else R.string.recently_closed_tabs
                ),
                numRecentTabs
            )
            binding.recentlyClosedNavEmpty.recentlyClosedNav.run {
                if (isNormalMode) {
                    isEnabled = true
                    alpha = 1f
                } else {
                    isEnabled = false
                    alpha = DELETE_BUTTON_DISABLED_ALPHA
                }
            }
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

        binding.historyLayout.attachMenu(historyMenu.menuController)
    }

    companion object {
        const val DELETE_BUTTON_DISABLED_ALPHA = 0.7f
        const val LAYOUT_ID = R.layout.history_list_item
    }
}

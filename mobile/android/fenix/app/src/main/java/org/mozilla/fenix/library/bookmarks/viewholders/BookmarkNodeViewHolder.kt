/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.viewholders

import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.removeAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentState
import org.mozilla.fenix.library.bookmarks.BookmarkItemMenu
import org.mozilla.fenix.library.bookmarks.BookmarkPayload
import org.mozilla.fenix.library.bookmarks.BookmarkViewInteractor
import org.mozilla.fenix.library.bookmarks.inRoots

/**
 * Base class for bookmark node view holders.
 */
class BookmarkNodeViewHolder(
    private val containerView: LibrarySiteItemView,
    private val interactor: BookmarkViewInteractor,
) : RecyclerView.ViewHolder(containerView) {

    var item: BookmarkNode? = null
    private val menu: BookmarkItemMenu

    init {
        menu = BookmarkItemMenu(containerView.context) { menuItem ->
            val item = this.item ?: return@BookmarkItemMenu
            when (menuItem) {
                BookmarkItemMenu.Item.Edit -> interactor.onEditPressed(item)
                BookmarkItemMenu.Item.Copy -> interactor.onCopyPressed(item)
                BookmarkItemMenu.Item.Share -> interactor.onSharePressed(item)
                BookmarkItemMenu.Item.OpenInNewTab -> interactor.onOpenInNormalTab(item)
                BookmarkItemMenu.Item.OpenInPrivateTab -> interactor.onOpenInPrivateTab(item)
                BookmarkItemMenu.Item.OpenAllInNewTabs -> interactor.onOpenAllInNewTabs(item)
                BookmarkItemMenu.Item.OpenAllInPrivateTabs -> interactor.onOpenAllInPrivateTabs(item)
                BookmarkItemMenu.Item.Delete -> interactor.onDelete(setOf(item))
            }
        }

        containerView.attachMenu(menu.menuController)
    }

    fun bind(
        item: BookmarkNode,
        mode: BookmarkFragmentState.Mode,
        payload: BookmarkPayload,
    ) {
        this.item = item

        containerView.urlView.isVisible = item.type == BookmarkNodeType.ITEM
        containerView.setSelectionInteractor(item, mode, interactor)

        CoroutineScope(Dispatchers.Default).launch {
            menu.updateMenu(item.type, item.guid)
        }

        // Hide menu button if this item is a root folder or is selected
        if (item.type == BookmarkNodeType.FOLDER && item.inRoots()) {
            containerView.overflowView.removeAndDisable()
        } else if (payload.modeChanged) {
            if (mode is BookmarkFragmentState.Mode.Selecting) {
                containerView.overflowView.hideAndDisable()
            } else {
                containerView.overflowView.showAndEnable()
            }
        }

        if (payload.selectedChanged) {
            containerView.changeSelected(item in mode.selectedItems)
        }

        val useTitleFallback = item.type == BookmarkNodeType.ITEM && item.title.isNullOrBlank()
        if (payload.titleChanged) {
            containerView.titleView.text = if (useTitleFallback) item.url else item.title
        } else if (payload.urlChanged && useTitleFallback) {
            containerView.titleView.text = item.url
        }

        if (payload.urlChanged) {
            containerView.urlView.text = item.url
        }

        if (payload.iconChanged) {
            updateIcon(item)
        }
    }

    private fun updateIcon(item: BookmarkNode) {
        val context = containerView.context
        val iconView = containerView.iconView
        val url = item.url

        when {
            // Item is a folder
            item.type == BookmarkNodeType.FOLDER ->
                iconView.setImageResource(R.drawable.ic_folder_icon)
            // Item has a http/https URL
            url != null && url.startsWith("http") ->
                context.components.core.icons.loadIntoView(iconView, url)
            else ->
                iconView.setImageDrawable(null)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.bookmark_list_item
    }
}

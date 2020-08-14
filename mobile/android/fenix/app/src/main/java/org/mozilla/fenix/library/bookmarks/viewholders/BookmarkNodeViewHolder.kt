/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.viewholders

import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.SelectionHolder
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentState
import org.mozilla.fenix.library.bookmarks.BookmarkItemMenu
import org.mozilla.fenix.library.bookmarks.BookmarkPayload
import org.mozilla.fenix.library.bookmarks.BookmarkViewInteractor
import org.mozilla.fenix.utils.Do

/**
 * Base class for bookmark node view holders.
 */
abstract class BookmarkNodeViewHolder(
    protected val containerView: LibrarySiteItemView,
    private val interactor: BookmarkViewInteractor
) : RecyclerView.ViewHolder(containerView) {

    abstract var item: BookmarkNode?
    private lateinit var menu: BookmarkItemMenu

    init {
        setupMenu()
    }

    abstract fun bind(item: BookmarkNode, mode: BookmarkFragmentState.Mode)

    abstract fun bind(
        item: BookmarkNode,
        mode: BookmarkFragmentState.Mode,
        payload: BookmarkPayload
    )

    protected fun setSelectionListeners(item: BookmarkNode, selectionHolder: SelectionHolder<BookmarkNode>) {
        containerView.setSelectionInteractor(item, selectionHolder, interactor)
    }

    private fun setupMenu() {
        menu = BookmarkItemMenu(containerView.context) { menuItem ->
            val item = this.item ?: return@BookmarkItemMenu
            Do exhaustive when (menuItem) {
                BookmarkItemMenu.Item.Edit -> interactor.onEditPressed(item)
                BookmarkItemMenu.Item.Copy -> interactor.onCopyPressed(item)
                BookmarkItemMenu.Item.Share -> interactor.onSharePressed(item)
                BookmarkItemMenu.Item.OpenInNewTab -> interactor.onOpenInNormalTab(item)
                BookmarkItemMenu.Item.OpenInPrivateTab -> interactor.onOpenInPrivateTab(item)
                BookmarkItemMenu.Item.Delete -> interactor.onDelete(setOf(item))
            }
        }

        containerView.attachMenu(menu.menuController)
    }

    protected fun updateMenu(itemType: BookmarkNodeType) = menu.updateMenu(itemType)
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.viewholders

import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.concept.storage.BookmarkNode
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.bookmarks.BookmarkItemMenu
import org.mozilla.fenix.library.bookmarks.BookmarkState
import org.mozilla.fenix.library.bookmarks.BookmarkViewInteractor

/**
 * Base class for bookmark node view holders.
 */
abstract class BookmarkNodeViewHolder(
    override val containerView: LibrarySiteItemView,
    val interactor: BookmarkViewInteractor
) :
    RecyclerView.ViewHolder(containerView), LayoutContainer {

    abstract fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean)

    protected fun setupMenu(item: BookmarkNode) {
        val bookmarkItemMenu = BookmarkItemMenu(containerView.context, item) {
            when (it) {
                BookmarkItemMenu.Item.Edit -> interactor.edit(item)
                BookmarkItemMenu.Item.Select -> interactor.select(item)
                BookmarkItemMenu.Item.Copy -> interactor.copy(item)
                BookmarkItemMenu.Item.Share -> interactor.share(item)
                BookmarkItemMenu.Item.OpenInNewTab -> interactor.openInNewTab(item)
                BookmarkItemMenu.Item.OpenInPrivateTab -> interactor.openInPrivateTab(item)
                BookmarkItemMenu.Item.Delete -> interactor.delete(item)
            }
        }

        containerView.overflowView.setOnClickListener {
            bookmarkItemMenu.menuBuilder.build(containerView.context).show(
                anchor = it,
                orientation = BrowserMenu.Orientation.DOWN
            )
        }
    }
}

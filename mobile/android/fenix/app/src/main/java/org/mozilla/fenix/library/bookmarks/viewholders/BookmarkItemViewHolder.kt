/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.viewholders

import mozilla.components.concept.storage.BookmarkNode
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.bookmarks.BookmarkState
import org.mozilla.fenix.library.bookmarks.BookmarkViewInteractor

/**
 * Represents a bookmarked website in the bookmarks page.
 */
class BookmarkItemViewHolder(
    view: LibrarySiteItemView,
    interactor: BookmarkViewInteractor
) : BookmarkNodeViewHolder(view, interactor) {

    override fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {

        containerView.displayAs(LibrarySiteItemView.ItemType.SITE)

        setupMenu(item)
        containerView.titleView.text = if (item.title.isNullOrBlank()) item.url else item.title
        containerView.urlView.text = item.url

        setClickListeners(mode, item, selected)
        containerView.changeSelected(selected)
        setColorsAndIcons(item.url)
    }

    private fun setColorsAndIcons(url: String?) {
        if (url != null && url.startsWith("http")) {
            containerView.loadFavicon(url)
        } else {
            containerView.iconView.setImageDrawable(null)
        }
    }

    private fun setClickListeners(
        mode: BookmarkState.Mode,
        item: BookmarkNode,
        selected: Boolean
    ) {
        containerView.setOnClickListener {
            when {
                mode == BookmarkState.Mode.Normal -> interactor.open(item)
                selected -> interactor.deselect(item)
                else -> interactor.select(item)
            }
        }

        containerView.setOnLongClickListener {
            if (mode == BookmarkState.Mode.Normal) {
                interactor.select(item)
                true
            } else false
        }

        containerView.iconView.setOnClickListener({
            when {
                selected -> interactor.deselect(item)
                else -> interactor.select(item)
            }
        })
    }
}

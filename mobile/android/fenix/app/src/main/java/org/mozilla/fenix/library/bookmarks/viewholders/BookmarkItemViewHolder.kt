/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.viewholders

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.storage.BookmarkNode
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentState
import org.mozilla.fenix.library.bookmarks.BookmarkPayload
import org.mozilla.fenix.library.bookmarks.BookmarkViewInteractor

/**
 * Represents a bookmarked website in the bookmarks page.
 */
class BookmarkItemViewHolder(
    view: LibrarySiteItemView,
    interactor: BookmarkViewInteractor
) : BookmarkNodeViewHolder(view, interactor) {

    override var item: BookmarkNode? = null

    init {
        containerView.displayAs(LibrarySiteItemView.ItemType.SITE)
    }

    override fun bind(
        item: BookmarkNode,
        mode: BookmarkFragmentState.Mode
    ) {
        bind(item, mode, BookmarkPayload(true, true, true, true))
    }

    override fun bind(item: BookmarkNode, mode: BookmarkFragmentState.Mode, payload: BookmarkPayload) {
        this.item = item

        setupMenu(item)

        if (payload.modeChanged) {
            if (mode is BookmarkFragmentState.Mode.Selecting) {
                containerView.overflowView.hideAndDisable()
            } else {
                containerView.overflowView.showAndEnable()
            }
        }

        if (payload.selectedChanged) {
            containerView.changeSelected(item in mode.selectedItems)
        }

        if (payload.titleChanged) {
            containerView.titleView.text = if (item.title.isNullOrBlank()) item.url else item.title
        } else if (payload.urlChanged && item.title.isNullOrBlank()) {
            containerView.titleView.text = item.url
        }

        if (payload.urlChanged) {
            containerView.urlView.text = item.url
            setColorsAndIcons(item.url)
        }

        setSelectionListeners(item, mode)
    }

    @VisibleForTesting
    internal fun setColorsAndIcons(url: String?) {
        if (url != null && url.startsWith("http")) {
            containerView.loadFavicon(url)
        } else {
            containerView.iconView.setImageDrawable(null)
        }
    }
}

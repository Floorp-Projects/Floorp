/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.viewholders

import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import mozilla.components.concept.storage.BookmarkNode
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.hideAndDisable
import org.mozilla.fenix.ext.showAndEnable
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.SelectionHolder
import org.mozilla.fenix.library.bookmarks.BookmarkFragmentState
import org.mozilla.fenix.library.bookmarks.BookmarkPayload
import org.mozilla.fenix.library.bookmarks.BookmarkViewInteractor
import org.mozilla.fenix.library.bookmarks.inRoots

/**
 * Represents a folder with other bookmarks inside.
 */
class BookmarkFolderViewHolder(
    view: LibrarySiteItemView,
    interactor: BookmarkViewInteractor,
    private val selectionHolder: SelectionHolder<BookmarkNode>
) : BookmarkNodeViewHolder(view, interactor) {

    override var item: BookmarkNode? = null

    init {
        containerView.displayAs(LibrarySiteItemView.ItemType.FOLDER)
    }

    override fun bind(
        item: BookmarkNode,
        mode: BookmarkFragmentState.Mode
    ) {
        bind(item, mode, BookmarkPayload(true, true, true, true))
    }

    override fun bind(item: BookmarkNode, mode: BookmarkFragmentState.Mode, payload: BookmarkPayload) {
        this.item = item

        setSelectionListeners(item, selectionHolder)

        if (!item.inRoots()) {
            setupMenu(item)
            if (payload.modeChanged) {
                if (mode is BookmarkFragmentState.Mode.Selecting) {
                    containerView.overflowView.hideAndDisable()
                } else {
                    containerView.overflowView.showAndEnable()
                }
            }
        } else {
            containerView.overflowView.visibility = View.GONE
        }

        if (payload.selectedChanged) {
            containerView.changeSelected(item in selectionHolder.selectedItems)
        }

        containerView.iconView.setImageDrawable(
            AppCompatResources.getDrawable(
                containerView.context,
                R.drawable.ic_folder_icon
            )?.apply {
                setTint(
                    ContextCompat.getColor(
                        containerView.context,
                        R.color.primary_text_light_theme
                    )
                )
            }
        )

        if (payload.titleChanged) {
            containerView.titleView.text = item.title
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import androidx.core.view.isVisible
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.SelectionHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkFolderViewHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkItemViewHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkNodeViewHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkSeparatorViewHolder

class BookmarkAdapter(val emptyView: View, val interactor: BookmarkViewInteractor) :
    RecyclerView.Adapter<BookmarkNodeViewHolder>(), SelectionHolder<BookmarkNode> {

    private var tree: List<BookmarkNode> = listOf()
    private var mode: BookmarkState.Mode = BookmarkState.Mode.Normal
    override val selectedItems: Set<BookmarkNode> get() = mode.selectedItems
    private var isFirstRun = true

    fun updateData(tree: BookmarkNode?, mode: BookmarkState.Mode) {
        val diffUtil = DiffUtil.calculateDiff(
            BookmarkDiffUtil(
                this.tree,
                tree?.children ?: listOf(),
                this.mode,
                mode
            )
        )

        this.tree = tree?.children ?: listOf()
        isFirstRun = if (isFirstRun) false else {
            emptyView.isVisible = this.tree.isEmpty()
            false
        }
        this.mode = mode

        diffUtil.dispatchUpdatesTo(this)
    }

    private class BookmarkDiffUtil(
        val old: List<BookmarkNode>,
        val new: List<BookmarkNode>,
        val oldMode: BookmarkState.Mode,
        val newMode: BookmarkState.Mode
    ) : DiffUtil.Callback() {
        override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
            old[oldItemPosition].guid == new[newItemPosition].guid

        override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
            old[oldItemPosition] in oldMode.selectedItems == new[newItemPosition] in newMode.selectedItems &&
                    old[oldItemPosition].title == new[newItemPosition].title &&
                    old[oldItemPosition].url == new[newItemPosition].url

        override fun getOldListSize(): Int = old.size
        override fun getNewListSize(): Int = new.size
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BookmarkNodeViewHolder {
        val view = LibrarySiteItemView(parent.context).apply {
            layoutParams = ViewGroup.LayoutParams(MATCH_PARENT, WRAP_CONTENT)
        }

        return when (viewType) {
            LibrarySiteItemView.ItemType.SITE.ordinal -> BookmarkItemViewHolder(view, interactor, this)
            LibrarySiteItemView.ItemType.FOLDER.ordinal -> BookmarkFolderViewHolder(view, interactor, this)
            LibrarySiteItemView.ItemType.SEPARATOR.ordinal -> BookmarkSeparatorViewHolder(view, interactor)
            else -> throw IllegalStateException("ViewType $viewType does not match to a ViewHolder")
        }
    }

    override fun getItemViewType(position: Int): Int {
        return when (tree[position].type) {
            BookmarkNodeType.ITEM -> LibrarySiteItemView.ItemType.SITE
            BookmarkNodeType.FOLDER -> LibrarySiteItemView.ItemType.FOLDER
            BookmarkNodeType.SEPARATOR -> LibrarySiteItemView.ItemType.SEPARATOR
            else -> throw IllegalStateException("Item $tree[position] does not match to a ViewType")
        }.ordinal
    }

    override fun getItemCount(): Int = tree.size

    override fun onBindViewHolder(holder: BookmarkNodeViewHolder, position: Int) {
        holder.bind(tree[position])
    }
}

fun BookmarkNode.inRoots() = enumValues<BookmarkRoot>().any { it.id == guid }

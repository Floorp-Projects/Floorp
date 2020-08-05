/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.SelectionHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkFolderViewHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkItemViewHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkNodeViewHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkSeparatorViewHolder

class BookmarkAdapter(val emptyView: View, val interactor: BookmarkViewInteractor) :
    RecyclerView.Adapter<BookmarkNodeViewHolder>(), SelectionHolder<BookmarkNode> {

    private var tree: List<BookmarkNode> = listOf()
    private var mode: BookmarkFragmentState.Mode = BookmarkFragmentState.Mode.Normal()
    override val selectedItems: Set<BookmarkNode> get() = mode.selectedItems
    private var isFirstRun = true

    fun updateData(tree: BookmarkNode?, mode: BookmarkFragmentState.Mode) {
        val diffUtil = DiffUtil.calculateDiff(
            BookmarkDiffUtil(
                this.tree,
                tree?.children.orEmpty(),
                this.mode,
                mode
            )
        )

        this.tree = tree?.children.orEmpty()
        isFirstRun = if (isFirstRun) false else {
            emptyView.isVisible = this.tree.isEmpty()
            false
        }
        this.mode = mode

        diffUtil.dispatchUpdatesTo(this)
    }

    @VisibleForTesting
    internal class BookmarkDiffUtil(
        val old: List<BookmarkNode>,
        val new: List<BookmarkNode>,
        val oldMode: BookmarkFragmentState.Mode,
        val newMode: BookmarkFragmentState.Mode
    ) : DiffUtil.Callback() {
        override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
            old[oldItemPosition].guid == new[newItemPosition].guid

        override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
            oldMode::class == newMode::class &&
            old[oldItemPosition] in oldMode.selectedItems == new[newItemPosition] in newMode.selectedItems &&
                    old[oldItemPosition] == new[newItemPosition]

        override fun getChangePayload(oldItemPosition: Int, newItemPosition: Int): Any? {
            val oldItem = old[oldItemPosition]
            val newItem = new[newItemPosition]
            return BookmarkPayload(
                titleChanged = oldItem.title != newItem.title,
                urlChanged = oldItem.url != newItem.url,
                selectedChanged = oldItem in oldMode.selectedItems != newItem in newMode.selectedItems,
                modeChanged = oldMode::class != newMode::class
            )
        }

        override fun getOldListSize(): Int = old.size
        override fun getNewListSize(): Int = new.size
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BookmarkNodeViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.bookmark_list_item, parent, false) as LibrarySiteItemView

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

    override fun onBindViewHolder(
        holder: BookmarkNodeViewHolder,
        position: Int,
        payloads: MutableList<Any>
    ) {
        if (payloads.isNotEmpty() && payloads[0] is BookmarkPayload) {
            holder.bind(tree[position], mode, payloads[0] as BookmarkPayload)
        } else {
            super.onBindViewHolder(holder, position, payloads)
        }
    }

    override fun onBindViewHolder(holder: BookmarkNodeViewHolder, position: Int) {
        holder.bind(tree[position], mode)
    }
}

/**
 * A RecyclerView Adapter payload class that contains information about changes to a [BookmarkNode].
 *
 * @property titleChanged true if there has been a change to [BookmarkNode.title].
 * @property urlChanged true if there has been a change to [BookmarkNode.url].
 * @property selectedChanged true if there has been a change in the BookmarkNode's selected state.
 * @property modeChanged true if there has been a change in the state's mode type.
 */
data class BookmarkPayload(
    val titleChanged: Boolean,
    val urlChanged: Boolean,
    val selectedChanged: Boolean,
    val modeChanged: Boolean
)

fun BookmarkNode.inRoots() = enumValues<BookmarkRoot>().any { it.id == guid }

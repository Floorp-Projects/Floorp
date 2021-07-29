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
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkNodeViewHolder
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkSeparatorViewHolder

class BookmarkAdapter(private val emptyView: View, private val interactor: BookmarkViewInteractor) :
    RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    @VisibleForTesting var tree: List<BookmarkNode> = listOf()
    private var mode: BookmarkFragmentState.Mode = BookmarkFragmentState.Mode.Normal()
    private var isFirstRun = true

    fun updateData(tree: BookmarkNode?, mode: BookmarkFragmentState.Mode) {
        val allNodes = tree?.children.orEmpty()
        val folders: MutableList<BookmarkNode> = mutableListOf()
        val notFolders: MutableList<BookmarkNode> = mutableListOf()
        val separators: MutableList<BookmarkNode> = mutableListOf()
        allNodes.forEach {
            when (it.type) {
                BookmarkNodeType.SEPARATOR -> separators.add(it)
                BookmarkNodeType.FOLDER -> folders.add(it)
                else -> notFolders.add(it)
            }
        }
        // Display folders above all other bookmarks. Exclude separators.
        // For separator removal, see discussion in https://github.com/mozilla-mobile/fenix/issues/15214
        val newTree = folders + notFolders - separators

        val diffUtil = DiffUtil.calculateDiff(
            BookmarkDiffUtil(
                this.tree,
                newTree,
                this.mode,
                mode
            )
        )

        this.tree = newTree

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
                modeChanged = oldMode::class != newMode::class,
                iconChanged = oldItem.type != newItem.type || oldItem.url != newItem.url
            )
        }

        override fun getOldListSize(): Int = old.size
        override fun getNewListSize(): Int = new.size
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)

        return when (viewType) {
            BookmarkNodeViewHolder.LAYOUT_ID ->
                BookmarkNodeViewHolder(view as LibrarySiteItemView, interactor)
            BookmarkSeparatorViewHolder.LAYOUT_ID ->
                BookmarkSeparatorViewHolder(view)
            else -> throw IllegalStateException("ViewType $viewType does not match to a ViewHolder")
        }
    }

    override fun getItemViewType(position: Int) = when (tree[position].type) {
        BookmarkNodeType.ITEM, BookmarkNodeType.FOLDER -> BookmarkNodeViewHolder.LAYOUT_ID
        BookmarkNodeType.SEPARATOR -> BookmarkSeparatorViewHolder.LAYOUT_ID
    }

    override fun getItemCount(): Int = tree.size

    override fun onBindViewHolder(
        holder: RecyclerView.ViewHolder,
        position: Int,
        payloads: MutableList<Any>
    ) {
        (holder as? BookmarkNodeViewHolder)?.apply {
            val diffPayload = if (payloads.isNotEmpty() && payloads[0] is BookmarkPayload) {
                payloads[0] as BookmarkPayload
            } else {
                BookmarkPayload()
            }
            bind(tree[position], mode, diffPayload)
        }
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        (holder as? BookmarkNodeViewHolder)?.bind(tree[position], mode, BookmarkPayload())
    }
}

/**
 * A RecyclerView Adapter payload class that contains information about changes to a [BookmarkNode].
 *
 * @property titleChanged true if there has been a change to [BookmarkNode.title].
 * @property urlChanged true if there has been a change to [BookmarkNode.url].
 * @property selectedChanged true if there has been a change in the BookmarkNode's selected state.
 * @property modeChanged true if there has been a change in the state's mode type.
 * @property iconChanged true if the icon displayed for the node should be changed.
 */
data class BookmarkPayload(
    val titleChanged: Boolean,
    val urlChanged: Boolean,
    val selectedChanged: Boolean,
    val modeChanged: Boolean,
    val iconChanged: Boolean
) {
    constructor() : this(
        titleChanged = true,
        urlChanged = true,
        selectedChanged = true,
        modeChanged = true,
        iconChanged = true
    )
}

fun BookmarkNode.inRoots() = enumValues<BookmarkRoot>().any { it.id == guid }

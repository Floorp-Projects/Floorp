/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.selectfolder

import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.ktx.android.util.dpToPx
import org.jetbrains.anko.image
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibrarySiteItemView
import org.mozilla.fenix.library.bookmarks.BookmarksSharedViewModel

class SelectBookmarkFolderAdapter(private val sharedViewModel: BookmarksSharedViewModel) :
    RecyclerView.Adapter<SelectBookmarkFolderAdapter.BookmarkFolderViewHolder>() {

    private var tree: List<BookmarkNodeWithDepth> = listOf()

    fun updateData(tree: BookmarkNode?) {
        this.tree = tree!!.convertToFolderDepthTree().drop(1)
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BookmarkFolderViewHolder {
        val view = LibrarySiteItemView(parent.context).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }

        return BookmarkFolderViewHolder(view)
    }

    override fun getItemCount(): Int = tree.size

    override fun onBindViewHolder(holder: BookmarkFolderViewHolder, position: Int) {
        holder.bind(
            tree[position],
            tree[position].node == sharedViewModel.selectedFolder
        ) { node ->
            sharedViewModel.apply {
                when (selectedFolder) {
                    node -> selectedFolder = null
                    else -> selectedFolder = node
                }
            }
            notifyDataSetChanged()
        }
    }

    class BookmarkFolderViewHolder(
        val view: LibrarySiteItemView
    ) :
        RecyclerView.ViewHolder(view), LayoutContainer {

        override val containerView get() = view

        init {
            view.displayAs(LibrarySiteItemView.ItemType.FOLDER)
            view.overflowView.visibility = View.GONE
        }

        fun bind(folder: BookmarkNodeWithDepth, selected: Boolean, onSelect: (BookmarkNode) -> Unit) {
            view.changeSelected(selected)
            view.iconView.image = containerView.context.getDrawable(R.drawable.ic_folder_icon)?.apply {
                setTint(ContextCompat.getColor(containerView.context, R.color.primary_text_light_theme))
            }
            view.titleView.text = folder.node.title
            view.setOnClickListener {
                onSelect(folder.node)
            }
            val pxToIndent = dpsToIndent.dpToPx(view.context.resources.displayMetrics)
            val padding = pxToIndent * if (folder.depth > maxDepth) maxDepth else folder.depth
            view.setPadding(padding, 0, 0, 0)
        }

        companion object {
            const val viewType = 1
        }
    }

    data class BookmarkNodeWithDepth(val depth: Int, val node: BookmarkNode, val parent: String?)

    private fun BookmarkNode.convertToFolderDepthTree(depth: Int = 0): List<BookmarkNodeWithDepth> {
        val newList = listOf(BookmarkNodeWithDepth(depth, this, this.parentGuid))
        return newList + children
            ?.filter { it.type == BookmarkNodeType.FOLDER }
            ?.flatMap { it.convertToFolderDepthTree(depth = depth + 1) }
            .orEmpty()
    }

    companion object {
        private const val maxDepth = 10
        private const val dpsToIndent = 10
    }
}

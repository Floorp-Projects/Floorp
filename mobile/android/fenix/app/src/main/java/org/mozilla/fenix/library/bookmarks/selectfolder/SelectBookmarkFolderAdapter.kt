/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.selectfolder

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintSet
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.bookmark_row.*
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getColorIntFromAttr
import org.mozilla.fenix.library.bookmarks.BookmarksSharedViewModel

class SelectBookmarkFolderAdapter(private val sharedViewModel: BookmarksSharedViewModel) :
    RecyclerView.Adapter<SelectBookmarkFolderAdapter.BookmarkFolderViewHolder>() {

    private var tree: List<BookmarkNodeWithDepth> = listOf()

    fun updateData(tree: BookmarkNode?) {
        this.tree = tree!!.convertToFolderDepthTree().drop(1)
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BookmarkFolderViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.bookmark_row, parent, false)

        return when (viewType) {
            BookmarkFolderViewHolder.viewType -> SelectBookmarkFolderAdapter.BookmarkFolderViewHolder(
                view
            )
            else -> throw IllegalStateException("ViewType $viewType does not match to a ViewHolder")
        }
    }

    override fun getItemViewType(position: Int): Int {
        return when (tree[position].node.type) {
            BookmarkNodeType.FOLDER -> BookmarkFolderViewHolder.viewType
            else -> throw IllegalStateException("Item $tree[position] does not match to a ViewType")
        }
    }

    override fun getItemCount(): Int = tree.size

    override fun onBindViewHolder(holder: BookmarkFolderViewHolder, position: Int) {
        holder.bind(
            tree[position],
            tree[position].node == sharedViewModel.selectedFolder,
            object : SelectionInterface {
                override fun itemSelected(node: BookmarkNode) {
                    sharedViewModel.apply {
                        when (selectedFolder) {
                            node -> selectedFolder = null
                            else -> selectedFolder = node
                        }
                    }
                    notifyDataSetChanged()
                }
            }
        )
    }

    interface SelectionInterface {
        fun itemSelected(node: BookmarkNode)
    }

    class BookmarkFolderViewHolder(
        view: View,
        override val containerView: View? = view
    ) :
        RecyclerView.ViewHolder(view), LayoutContainer {

        init {
            bookmark_favicon.visibility = View.VISIBLE
            bookmark_title.visibility = View.VISIBLE
            bookmark_url.visibility = View.GONE
            bookmark_separator.visibility = View.GONE
            bookmark_layout.isClickable = true
        }

        fun bind(folder: BookmarkNodeWithDepth, selected: Boolean, selectionInterface: SelectionInterface) {
            val backgroundTint =
                if (selected) {
                    R.attr.accentBright.getColorIntFromAttr(containerView!!.context)
                } else {
                    R.attr.neutral.getColorIntFromAttr(containerView!!.context)
                }

            // Center the bookmark title since we don't have a url
            val constraintSet = ConstraintSet()
            constraintSet.clone(bookmark_layout)
            constraintSet.connect(
                bookmark_title.id, ConstraintSet.BOTTOM, ConstraintSet.PARENT_ID, ConstraintSet.BOTTOM
            )
            constraintSet.applyTo(bookmark_layout)

            val backgroundTintList = ContextCompat.getColorStateList(containerView.context, backgroundTint)
            bookmark_favicon.backgroundTintList = backgroundTintList
            val res = if (selected) R.drawable.mozac_ic_check else R.drawable.ic_folder_icon
            bookmark_favicon.setImageResource(res)
            bookmark_overflow.visibility = View.GONE
            bookmark_title?.text = folder.node.title
            bookmark_layout.setOnClickListener {
                selectionInterface.itemSelected(folder.node)
            }
            val pxToIndent = dpsToIndent.dpToPx(containerView.resources.displayMetrics)
            val padding = pxToIndent * if (folder.depth > maxDepth) maxDepth else folder.depth
            bookmark_layout.setPadding(padding, 0, 0, 0)
        }

        companion object {
            const val viewType = 1
        }
    }

    data class BookmarkNodeWithDepth(val depth: Int, val node: BookmarkNode, val parent: String?)

    private fun BookmarkNode?.convertToFolderDepthTree(
        depth: Int = 0,
        list: List<BookmarkNodeWithDepth> = listOf()
    ): List<BookmarkNodeWithDepth> {
        return if (this != null) {
            val newList = list.plus(listOf(BookmarkNodeWithDepth(depth, this, this.parentGuid)))
            newList.plus(
                children?.filter { it.type == BookmarkNodeType.FOLDER }
                    ?.flatMap { it.convertToFolderDepthTree(depth + 1) }
                    ?: listOf())
        } else listOf()
    }

    companion object {
        private const val maxDepth = 10
        private const val dpsToIndent = 10
    }
}

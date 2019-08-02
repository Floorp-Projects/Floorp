/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.jetbrains.anko.image
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibrarySiteItemView

class BookmarkAdapter(val emptyView: View, val interactor: BookmarkViewInteractor) :
    RecyclerView.Adapter<BookmarkAdapter.BookmarkNodeViewHolder>() {

    private var tree: List<BookmarkNode> = listOf()
    private var mode: BookmarkState.Mode = BookmarkState.Mode.Normal
    val selected: Set<BookmarkNode>
        get() = (mode as? BookmarkState.Mode.Selecting)?.selectedItems ?: setOf()
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
            emptyView.visibility = if (this.tree.isEmpty()) View.VISIBLE else View.GONE
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

        override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean {
            val oldSelected = (oldMode as? BookmarkState.Mode.Selecting)?.selectedItems ?: setOf()
            val newSelected = (newMode as? BookmarkState.Mode.Selecting)?.selectedItems ?: setOf()
            val modesEqual = oldMode::class == newMode::class
            val selectedEqual =
                ((oldSelected.contains(old[oldItemPosition]) && newSelected.contains(new[newItemPosition])) ||
                        (!oldSelected.contains(old[oldItemPosition]) && !newSelected.contains(new[newItemPosition])))
            return modesEqual && selectedEqual
        }

        override fun getOldListSize(): Int = old.size
        override fun getNewListSize(): Int = new.size
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BookmarkNodeViewHolder {
        val view = LibrarySiteItemView(parent.context).apply {
            layoutParams = ViewGroup.LayoutParams(MATCH_PARENT, WRAP_CONTENT)
        }

        return when (viewType) {
            LibrarySiteItemView.ItemType.SITE.ordinal ->
                BookmarkItemViewHolder(view, interactor)
            LibrarySiteItemView.ItemType.FOLDER.ordinal ->
                BookmarkFolderViewHolder(view, interactor)
            LibrarySiteItemView.ItemType.SEPARATOR.ordinal ->
                BookmarkSeparatorViewHolder(view, interactor)
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
        holder.bind(
            tree[position],
            mode,
            tree[position] in selected
        )
    }

    abstract class BookmarkNodeViewHolder(
        val view: LibrarySiteItemView,
        val interactor: BookmarkViewInteractor
    ) :
        RecyclerView.ViewHolder(view), LayoutContainer {

        override val containerView get() = view

        abstract fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean)

        protected fun setupMenu(item: BookmarkNode) {
            val bookmarkItemMenu = BookmarkItemMenu(view.context, item) {
                when (it) {
                    is BookmarkItemMenu.Item.Edit -> interactor.edit(item)
                    is BookmarkItemMenu.Item.Select -> interactor.select(item)
                    is BookmarkItemMenu.Item.Copy -> interactor.copy(item)
                    is BookmarkItemMenu.Item.Share -> interactor.share(item)
                    is BookmarkItemMenu.Item.OpenInNewTab -> interactor.openInNewTab(item)
                    is BookmarkItemMenu.Item.OpenInPrivateTab -> interactor.openInPrivateTab(item)
                    is BookmarkItemMenu.Item.Delete -> interactor.delete(item)
                }
            }

            view.overflowView.setOnClickListener {
                bookmarkItemMenu.menuBuilder.build(view.context).show(
                    anchor = it,
                    orientation = BrowserMenu.Orientation.DOWN
                )
            }
        }
    }

    class BookmarkItemViewHolder(
        view: LibrarySiteItemView,
        interactor: BookmarkViewInteractor
    ) :
        BookmarkNodeViewHolder(view, interactor) {

        @Suppress("ComplexMethod")
        override fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {

            view.displayAs(LibrarySiteItemView.ItemType.SITE)

            setupMenu(item)
            view.titleView.text = if (item.title.isNullOrBlank()) item.url else item.title
            view.urlView.text = item.url

            setClickListeners(mode, item, selected)
            view.changeSelected(selected)
            setColorsAndIcons(item.url)
        }

        private fun setColorsAndIcons(url: String?) {
            if (url != null && url.startsWith("http")) {
                view.loadFavicon(url)
            } else {
                view.iconView.setImageDrawable(null)
            }
        }

        private fun setClickListeners(
            mode: BookmarkState.Mode,
            item: BookmarkNode,
            selected: Boolean
        ) {
            view.setOnClickListener {
                when {
                    mode == BookmarkState.Mode.Normal -> interactor.open(item)
                    selected -> interactor.deselect(item)
                    else -> interactor.select(item)
                }
            }

            view.setOnLongClickListener {
                if (mode == BookmarkState.Mode.Normal) {
                    interactor.select(item)
                    true
                } else false
            }
        }
    }

    class BookmarkFolderViewHolder(
        view: LibrarySiteItemView,
        interactor: BookmarkViewInteractor
    ) :
        BookmarkNodeViewHolder(view, interactor) {

        override fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {

            view.displayAs(LibrarySiteItemView.ItemType.FOLDER)

            setClickListeners(mode, item, selected)

            if (!item.inRoots()) {
                setupMenu(item)
            } else {
                view.overflowView.visibility = View.GONE
            }

            view.changeSelected(selected)
            view.iconView.image = view.context.getDrawable(R.drawable.ic_folder_icon)?.apply {
                setTint(ContextCompat.getColor(view.context, R.color.primary_text_light_theme))
            }
            view.titleView.text = item.title
        }

        private fun setClickListeners(
            mode: BookmarkState.Mode,
            item: BookmarkNode,
            selected: Boolean
        ) {
            view.setOnClickListener {
                when {
                    mode == BookmarkState.Mode.Normal -> interactor.expand(item)
                    selected -> interactor.deselect(item)
                    else -> interactor.select(item)
                }
            }

            view.setOnLongClickListener {
                if (mode == BookmarkState.Mode.Normal && !item.inRoots()) {
                    interactor.select(item)
                    true
                } else false
            }
        }
    }

    class BookmarkSeparatorViewHolder(
        view: LibrarySiteItemView,
        interactor: BookmarkViewInteractor
    ) : BookmarkNodeViewHolder(view, interactor) {

        override fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {
            view.displayAs(LibrarySiteItemView.ItemType.SEPARATOR)
            setupMenu(item)
        }
    }
}

fun BookmarkNode.inRoots() = enumValues<BookmarkRoot>().any { it.id == guid }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.util.TypedValue
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.bookmark_row.*
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ext.loadIntoView

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
        val view = LayoutInflater.from(parent.context).inflate(R.layout.bookmark_row, parent, false)

        return when (viewType) {
            BookmarkItemViewHolder.viewType.ordinal -> BookmarkItemViewHolder(
                view, interactor
            )
            BookmarkFolderViewHolder.viewType.ordinal -> BookmarkFolderViewHolder(
                view, interactor
            )
            BookmarkSeparatorViewHolder.viewType.ordinal -> BookmarkSeparatorViewHolder(
                view, interactor
            )
            else -> throw IllegalStateException("ViewType $viewType does not match to a ViewHolder")
        }
    }

    override fun getItemViewType(position: Int): Int {
        return when (tree[position].type) {
            BookmarkNodeType.ITEM -> ViewType.ITEM.ordinal
            BookmarkNodeType.FOLDER -> ViewType.FOLDER.ordinal
            BookmarkNodeType.SEPARATOR -> ViewType.SEPARATOR.ordinal
            else -> throw IllegalStateException("Item $tree[position] does not match to a ViewType")
        }
    }

    override fun getItemCount(): Int = tree.size

    override fun onBindViewHolder(holder: BookmarkNodeViewHolder, position: Int) {
        holder.bind(
            tree[position],
            mode,
            tree[position] in selected
        )
    }

    open class BookmarkNodeViewHolder(
        view: View,
        val interactor: BookmarkViewInteractor,
        override val containerView: View? = view
    ) : RecyclerView.ViewHolder(view), LayoutContainer {

        open fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {}
    }

    class BookmarkItemViewHolder(
        view: View,
        interactor: BookmarkViewInteractor,
        override val containerView: View? = view
    ) :
        BookmarkNodeViewHolder(view, interactor, containerView) {

        @Suppress("ComplexMethod")
        override fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {

            val shiftTwoDp = TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, TWO_DIGIT_MARGIN, containerView!!.context.resources.displayMetrics
            ).toInt()
            val params = bookmark_title.layoutParams as ViewGroup.MarginLayoutParams
            params.topMargin = shiftTwoDp
            bookmark_title.layoutParams = params

            bookmark_favicon.visibility = View.VISIBLE
            bookmark_title.visibility = View.VISIBLE
            bookmark_url.visibility = View.VISIBLE
            bookmark_overflow.visibility = View.VISIBLE
            bookmark_separator.visibility = View.GONE
            bookmark_layout.isClickable = true

            val bookmarkItemMenu = BookmarkItemMenu(containerView.context, item) {
                when (it) {
                    is BookmarkItemMenu.Item.Edit -> {
                        interactor.edit(item)
                    }
                    is BookmarkItemMenu.Item.Select -> {
                        interactor.select(item)
                    }
                    is BookmarkItemMenu.Item.Copy -> {
                        interactor.copy(item)
                    }
                    is BookmarkItemMenu.Item.Share -> {
                        interactor.share(item)
                    }
                    is BookmarkItemMenu.Item.OpenInNewTab -> {
                        interactor.openInNewTab(item)
                    }
                    is BookmarkItemMenu.Item.OpenInPrivateTab -> {
                        interactor.openInPrivateTab(item)
                    }
                    is BookmarkItemMenu.Item.Delete -> {
                        interactor.delete(item)
                    }
                }
            }

            bookmark_overflow.increaseTapArea(bookmarkOverflowExtraDips)
            bookmark_overflow.setOnClickListener {
                bookmarkItemMenu.menuBuilder.build(containerView.context).show(anchor = it)
            }
            bookmark_title.text = if (item.title.isNullOrBlank()) item.url else item.title
            bookmark_url.text = item.url
            updateUrl(item, mode, selected)
        }

        private fun updateUrl(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {
            setClickListeners(mode, item, selected)

            setColorsAndIcons(selected, item)
        }

        private fun setColorsAndIcons(selected: Boolean, item: BookmarkNode) {
            val backgroundTint =
                if (selected) {
                    ThemeManager.resolveAttribute(R.attr.accentHighContrast, containerView!!.context)
                } else {
                    ThemeManager.resolveAttribute(R.attr.neutral, containerView!!.context)
                }

            val backgroundTintList = ContextCompat.getColorStateList(containerView.context, backgroundTint)
            bookmark_favicon.backgroundTintList = backgroundTintList
            if (selected) bookmark_favicon.setImageResource(R.drawable.mozac_ic_check)

            val url = item.url ?: return
            if (!selected && url.startsWith("http")) {
                bookmark_layout.context.components.core.icons.loadIntoView(bookmark_favicon, url)
            }
        }

        private fun setClickListeners(
            mode: BookmarkState.Mode,
            item: BookmarkNode,
            selected: Boolean
        ) {
            bookmark_layout.setOnClickListener {
                if (mode == BookmarkState.Mode.Normal) {
                    interactor.open(item)
                } else {
                    if (selected) interactor.deselect(item) else interactor.select(item)
                }
            }

            bookmark_layout.setOnLongClickListener {
                if (mode == BookmarkState.Mode.Normal) {
                    if (selected) interactor.deselect(item) else interactor.select(item)
                    true
                } else false
            }
        }

        companion object {
            internal const val TWO_DIGIT_MARGIN = 2F

            val viewType = ViewType.ITEM
        }
    }

    class BookmarkFolderViewHolder(
        view: View,
        interactor: BookmarkViewInteractor,
        override val containerView: View? = view
    ) :
        BookmarkNodeViewHolder(view, interactor, containerView) {

        override fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {
            containerView?.context?.let {
                val drawable = it.getDrawable(R.drawable.ic_folder_icon)
                drawable?.setTint(
                    ContextCompat.getColor(
                        it,
                        R.color.primary_text_light_theme
                    )
                )
                bookmark_favicon.setImageDrawable(drawable)
            }
            bookmark_favicon.visibility = View.VISIBLE
            bookmark_title.visibility = View.VISIBLE
            bookmark_url.visibility = View.GONE
            bookmark_overflow.visibility = View.VISIBLE
            bookmark_separator.visibility = View.GONE
            bookmark_layout.isClickable = true

            setClickListeners(mode, item, selected)

            setMenu(item, containerView!!)

            val backgroundTint = if (selected) {
                ThemeManager.resolveAttribute(R.attr.accentHighContrast, containerView.context)
            } else {
                ThemeManager.resolveAttribute(R.attr.neutral, containerView.context)
            }

            val backgroundTintList = ContextCompat.getColorStateList(containerView.context, backgroundTint)
            bookmark_favicon.backgroundTintList = backgroundTintList
            val res = if (selected) R.drawable.mozac_ic_check else R.drawable.ic_folder_icon
            bookmark_favicon.setImageResource(res)

            bookmark_title?.text = item.title
        }

        private fun setMenu(
            item: BookmarkNode,
            containerView: View
        ) {
            val bookmarkItemMenu = BookmarkItemMenu(containerView.context, item) {
                when (it) {
                    is BookmarkItemMenu.Item.Edit -> {
                        interactor.edit(item)
                    }
                    is BookmarkItemMenu.Item.Select -> {
                        interactor.select(item)
                    }
                    is BookmarkItemMenu.Item.Delete -> {
                        interactor.delete(item)
                    }
                }
            }

            if (!item.inRoots()) {
                bookmark_overflow.increaseTapArea(bookmarkOverflowExtraDips)
                bookmark_overflow.setOnClickListener {
                    bookmarkItemMenu.menuBuilder.build(containerView.context).show(
                        anchor = it,
                        orientation = BrowserMenu.Orientation.DOWN
                    )
                }
                bookmark_layout.setOnLongClickListener(null)
            } else {
                bookmark_overflow.visibility = View.GONE
            }
        }

        private fun setClickListeners(
            mode: BookmarkState.Mode,
            item: BookmarkNode,
            selected: Boolean
        ) {
            bookmark_layout.setOnClickListener {
                if (mode == BookmarkState.Mode.Normal) {
                    interactor.expand(item)
                } else {
                    if (selected) interactor.deselect(item) else interactor.select(item)
                }
            }

            bookmark_layout.setOnLongClickListener {
                if (mode == BookmarkState.Mode.Normal && !item.inRoots()) {
                    if (selected) interactor.deselect(item) else interactor.select(item)
                    true
                } else false
            }
        }

        companion object {
            val viewType = ViewType.FOLDER
        }
    }

    class BookmarkSeparatorViewHolder(
        view: View,
        interactor: BookmarkViewInteractor,
        override val containerView: View? = view
    ) : BookmarkNodeViewHolder(view, interactor, containerView) {

        override fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {

            bookmark_favicon.visibility = View.GONE
            bookmark_title.visibility = View.GONE
            bookmark_url.visibility = View.GONE
            bookmark_overflow.increaseTapArea(bookmarkOverflowExtraDips)
            bookmark_overflow.visibility = View.GONE
            bookmark_separator.visibility = View.VISIBLE
            bookmark_layout.isClickable = false

            val bookmarkItemMenu = BookmarkItemMenu(containerView!!.context, item) {
                when (it) {
                    is BookmarkItemMenu.Item.Delete -> {
                        interactor.delete(item)
                    }
                }
            }

            bookmark_overflow.setOnClickListener {
                bookmarkItemMenu.menuBuilder.build(containerView.context).show(
                    anchor = it,
                    orientation = BrowserMenu.Orientation.DOWN
                )
            }
        }

        companion object {
            val viewType = ViewType.SEPARATOR
        }
    }

    companion object {
        private const val bookmarkOverflowExtraDips = 16
    }

    enum class ViewType {
        ITEM, FOLDER, SEPARATOR
    }
}

fun BookmarkNode.inRoots() = enumValues<BookmarkRoot>().any { it.id == guid }

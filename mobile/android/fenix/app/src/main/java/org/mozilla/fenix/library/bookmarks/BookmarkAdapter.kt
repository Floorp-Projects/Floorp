/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.util.TypedValue
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintSet
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.bookmark_row.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.R
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.increaseTapArea
import kotlin.coroutines.CoroutineContext

class BookmarkAdapter(val emptyView: View, val actionEmitter: Observer<BookmarkAction>) :
    RecyclerView.Adapter<BookmarkAdapter.BookmarkNodeViewHolder>() {

    private var tree: List<BookmarkNode> = listOf()
    private var mode: BookmarkState.Mode = BookmarkState.Mode.Normal
    val selected: Set<BookmarkNode>
        get() = (mode as? BookmarkState.Mode.Selecting)?.selectedItems ?: setOf()
    private var isFirstRun = true

    lateinit var job: Job

    fun updateData(tree: BookmarkNode?, mode: BookmarkState.Mode) {
        this.tree = tree?.children ?: listOf()
        isFirstRun = if (isFirstRun) false else {
            emptyView.visibility = if (this.tree.isEmpty()) View.VISIBLE else View.GONE
            false
        }
        this.mode = mode
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BookmarkNodeViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.bookmark_row, parent, false)

        return when (viewType) {
            BookmarkItemViewHolder.viewType.ordinal -> BookmarkItemViewHolder(
                view, actionEmitter, job
            )
            BookmarkFolderViewHolder.viewType.ordinal -> BookmarkFolderViewHolder(
                view, actionEmitter, job
            )
            BookmarkSeparatorViewHolder.viewType.ordinal -> BookmarkSeparatorViewHolder(
                view, actionEmitter, job
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

    override fun onAttachedToRecyclerView(recyclerView: RecyclerView) {
        super.onAttachedToRecyclerView(recyclerView)
        job = Job()
    }

    override fun onDetachedFromRecyclerView(recyclerView: RecyclerView) {
        super.onDetachedFromRecyclerView(recyclerView)
        job.cancel()
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
        val actionEmitter: Observer<BookmarkAction>,
        private val job: Job,
        override val containerView: View? = view
    ) :
        RecyclerView.ViewHolder(view), LayoutContainer, CoroutineScope {

        override val coroutineContext: CoroutineContext
            get() = Dispatchers.Main + job

        open fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {}
    }

    class BookmarkItemViewHolder(
        view: View,
        actionEmitter: Observer<BookmarkAction>,
        job: Job,
        override val containerView: View? = view
    ) :
        BookmarkNodeViewHolder(view, actionEmitter, job, containerView) {

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
                        actionEmitter.onNext(BookmarkAction.Edit(item))
                    }
                    is BookmarkItemMenu.Item.Select -> {
                        actionEmitter.onNext(BookmarkAction.Select(item))
                    }
                    is BookmarkItemMenu.Item.Copy -> {
                        actionEmitter.onNext(BookmarkAction.Copy(item))
                    }
                    is BookmarkItemMenu.Item.Share -> {
                        actionEmitter.onNext(BookmarkAction.Share(item))
                    }
                    is BookmarkItemMenu.Item.OpenInNewTab -> {
                        actionEmitter.onNext(BookmarkAction.OpenInNewTab(item))
                    }
                    is BookmarkItemMenu.Item.OpenInPrivateTab -> {
                        actionEmitter.onNext(BookmarkAction.OpenInPrivateTab(item))
                    }
                    is BookmarkItemMenu.Item.Delete -> {
                        actionEmitter.onNext(BookmarkAction.Delete(item))
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

            if (!selected && item.url?.startsWith("http") == true) {
                launch(Dispatchers.IO) {
                    val bitmap = bookmark_layout.context.components.core.icons
                        .loadIcon(IconRequest(item.url!!)).await().bitmap
                    launch(Dispatchers.Main) {
                        bookmark_favicon.setImageBitmap(bitmap)
                    }
                }
            }
        }

        private fun setClickListeners(
            mode: BookmarkState.Mode,
            item: BookmarkNode,
            selected: Boolean
        ) {
            bookmark_layout.setOnClickListener {
                if (mode == BookmarkState.Mode.Normal) {
                    actionEmitter.onNext(BookmarkAction.Open(item))
                } else {
                    if (selected) actionEmitter.onNext(BookmarkAction.Deselect(item)) else actionEmitter.onNext(
                        BookmarkAction.Select(item)
                    )
                }
            }

            bookmark_layout.setOnLongClickListener {
                if (mode == BookmarkState.Mode.Normal) {
                    if (selected) actionEmitter.onNext(BookmarkAction.Deselect(item)) else actionEmitter.onNext(
                        BookmarkAction.Select(item)
                    )
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
        actionEmitter: Observer<BookmarkAction>,
        job: Job,
        override val containerView: View? = view
    ) :
        BookmarkNodeViewHolder(view, actionEmitter, job, containerView) {

        override fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {

            val constraintSet = ConstraintSet()
            constraintSet.clone(bookmark_layout)
            constraintSet.connect(
                bookmark_title.id, ConstraintSet.BOTTOM, ConstraintSet.PARENT_ID, ConstraintSet.BOTTOM
            )
            constraintSet.applyTo(bookmark_layout)

            bookmark_favicon.setImageResource(R.drawable.ic_folder_icon)
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
                        actionEmitter.onNext(BookmarkAction.Edit(item))
                    }
                    is BookmarkItemMenu.Item.Select -> {
                        actionEmitter.onNext(BookmarkAction.Select(item))
                    }
                    is BookmarkItemMenu.Item.Delete -> {
                        actionEmitter.onNext(BookmarkAction.Delete(item))
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
                    actionEmitter.onNext(BookmarkAction.Expand(item))
                } else {
                    if (selected) actionEmitter.onNext(BookmarkAction.Deselect(item)) else actionEmitter.onNext(
                        BookmarkAction.Select(item)
                    )
                }
            }

            bookmark_layout.setOnLongClickListener {
                if (mode == BookmarkState.Mode.Normal && !item.inRoots()) {
                    if (selected) actionEmitter.onNext(BookmarkAction.Deselect(item)) else actionEmitter.onNext(
                        BookmarkAction.Select(item)
                    )
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
        actionEmitter: Observer<BookmarkAction>,
        job: Job,
        override val containerView: View? = view
    ) : BookmarkNodeViewHolder(view, actionEmitter, job, containerView) {

        override fun bind(item: BookmarkNode, mode: BookmarkState.Mode, selected: Boolean) {

            bookmark_favicon.visibility = View.GONE
            bookmark_title.visibility = View.GONE
            bookmark_url.visibility = View.GONE
            bookmark_overflow.increaseTapArea(bookmarkOverflowExtraDips)
            bookmark_overflow.visibility = View.VISIBLE
            bookmark_separator.visibility = View.VISIBLE
            bookmark_layout.isClickable = false

            val bookmarkItemMenu = BookmarkItemMenu(containerView!!.context, item) {
                when (it) {
                    is BookmarkItemMenu.Item.Delete -> {
                        actionEmitter.onNext(BookmarkAction.Delete(item))
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

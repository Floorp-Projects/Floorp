/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.bookmark_row.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.R
import kotlin.coroutines.CoroutineContext

class BookmarkAdapter(val actionEmitter: Observer<BookmarkAction>) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    private var tree: List<BookmarkNode> = listOf()
    private var mode: BookmarkState.Mode = BookmarkState.Mode.Normal

    lateinit var job: Job

    fun updateData(tree: BookmarkNode?, mode: BookmarkState.Mode) {
        this.tree = tree?.children?.filterNotNull() ?: listOf()
        this.mode = mode
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.bookmark_row, parent, false)

        return when (viewType) {
            BookmarkItemViewHolder.viewType.ordinal -> BookmarkAdapter.BookmarkItemViewHolder(
                view, actionEmitter, job
            )
            BookmarkFolderViewHolder.viewType.ordinal -> BookmarkAdapter.BookmarkFolderViewHolder(
                view, actionEmitter
            )
            BookmarkSeparatorViewHolder.viewType.ordinal -> BookmarkAdapter.BookmarkSeparatorViewHolder(
                view, actionEmitter
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

    @SuppressWarnings("ComplexMethod")
    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {

        val bookmarkItemMenu = BookmarkItemMenu(holder.itemView.context) {
            when (it) {
                is BookmarkItemMenu.Item.Edit -> {
                    actionEmitter.onNext(BookmarkAction.Edit(tree[position]))
                }
                is BookmarkItemMenu.Item.Select -> {
                    actionEmitter.onNext(BookmarkAction.Select(tree[position]))
                }
                is BookmarkItemMenu.Item.Copy -> {
                    actionEmitter.onNext(BookmarkAction.Copy(tree[position]))
                }
                is BookmarkItemMenu.Item.Share -> {
                    actionEmitter.onNext(BookmarkAction.Share(tree[position]))
                }
                is BookmarkItemMenu.Item.OpenInNewTab -> {
                    actionEmitter.onNext(BookmarkAction.OpenInNewTab(tree[position]))
                }
                is BookmarkItemMenu.Item.OpenInPrivateTab -> {
                    actionEmitter.onNext(BookmarkAction.OpenInPrivateTab(tree[position]))
                }
                is BookmarkItemMenu.Item.Delete -> {
                    actionEmitter.onNext(BookmarkAction.Delete(tree[position]))
                }
            }
        }

        when (holder) {
            is BookmarkAdapter.BookmarkItemViewHolder -> holder.bind(tree[position], bookmarkItemMenu, mode)
            is BookmarkAdapter.BookmarkFolderViewHolder -> holder.bind(tree[position], bookmarkItemMenu, mode)
            is BookmarkAdapter.BookmarkSeparatorViewHolder -> holder.bind(bookmarkItemMenu)
        }
    }

    class BookmarkItemViewHolder(
        view: View,
        val actionEmitter: Observer<BookmarkAction>,
        private val job: Job,
        override val containerView: View? = view
    ) :
        RecyclerView.ViewHolder(view), LayoutContainer, CoroutineScope {

        override val coroutineContext: CoroutineContext
            get() = Dispatchers.Main + job

        private var item: BookmarkNode? = null
        private var mode: BookmarkState.Mode? = BookmarkState.Mode.Normal

        init {
            bookmark_favicon.visibility = View.VISIBLE
            bookmark_title.visibility = View.VISIBLE
            bookmark_overflow.visibility = View.VISIBLE
            bookmark_separator.visibility = View.GONE
            bookmark_layout.isClickable = true
        }

        fun bind(item: BookmarkNode, bookmarkItemMenu: BookmarkItemMenu, mode: BookmarkState.Mode) {
            this.item = item
            this.mode = mode

            bookmark_overflow.setOnClickListener {
                bookmarkItemMenu.menuBuilder.build(containerView!!.context).show(
                    anchor = it,
                    orientation = BrowserMenu.Orientation.DOWN
                )
            }
            bookmark_title.text = item.title
            updateUrl(item)
        }

        private fun updateUrl(item: BookmarkNode) {
            bookmark_layout.setOnClickListener {
                if (mode == BookmarkState.Mode.Normal) {
                    actionEmitter.onNext(BookmarkAction.Open(item))
                } else {
                    actionEmitter.onNext(BookmarkAction.Select(item))
                }
            }

            bookmark_layout.setOnLongClickListener {
                if (mode == BookmarkState.Mode.Normal) {
                    actionEmitter.onNext(BookmarkAction.Select(item))
                    true
                } else false
            }

            if (item.url?.startsWith("http") == true) {
                launch(Dispatchers.IO) {
                    val bitmap = BrowserIcons(bookmark_favicon.context)
                        .loadIcon(IconRequest(item.url!!)).await().bitmap
                    launch(Dispatchers.Main) {
                        bookmark_favicon.setImageBitmap(bitmap)
                    }
                }
            }
        }

        companion object {
            val viewType = BookmarkAdapter.ViewType.ITEM
        }
    }

    class BookmarkFolderViewHolder(
        view: View,
        val actionEmitter: Observer<BookmarkAction>,
        override val containerView: View? = view
    ) :
        RecyclerView.ViewHolder(view), LayoutContainer {

        init {
            bookmark_favicon.setImageResource(R.drawable.ic_folder_icon)
            bookmark_favicon.visibility = View.VISIBLE
            bookmark_title.visibility = View.VISIBLE
            bookmark_overflow.visibility = View.VISIBLE
            bookmark_separator.visibility = View.GONE
            bookmark_layout.isClickable = true
        }

        fun bind(folder: BookmarkNode, bookmarkItemMenu: BookmarkItemMenu, mode: BookmarkState.Mode) {
            bookmark_overflow.setOnClickListener {
                bookmarkItemMenu.menuBuilder.build(containerView!!.context).show(
                    anchor = it,
                    orientation = BrowserMenu.Orientation.DOWN
                )
            }
            bookmark_title?.text = folder.title
            bookmark_layout.setOnClickListener {
                actionEmitter.onNext(BookmarkAction.Expand(folder))
            }
        }

        companion object {
            val viewType = BookmarkAdapter.ViewType.FOLDER
        }
    }

    class BookmarkSeparatorViewHolder(
        view: View,
        val actionEmitter: Observer<BookmarkAction>,
        override val containerView: View? = view
    ) : RecyclerView.ViewHolder(view), LayoutContainer {

        init {
            bookmark_favicon.visibility = View.GONE
            bookmark_title.visibility = View.GONE
            bookmark_overflow.visibility = View.VISIBLE
            bookmark_separator.visibility = View.VISIBLE
            bookmark_layout.isClickable = false
        }

        fun bind(bookmarkItemMenu: BookmarkItemMenu) {
            bookmark_overflow.setOnClickListener {
                bookmarkItemMenu.menuBuilder.build(containerView!!.context).show(
                    anchor = it,
                    orientation = BrowserMenu.Orientation.DOWN
                )
            }
        }

        companion object {
            val viewType = BookmarkAdapter.ViewType.SEPARATOR
        }
    }

    enum class ViewType {
        ITEM, FOLDER, SEPARATOR
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.graphics.PorterDuff.Mode.SRC_IN
import android.graphics.PorterDuffColorFilter
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.view.menu.ActionMenuItemView
import androidx.appcompat.widget.ActionMenuView
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_bookmark.view.*
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.getColorIntFromAttr
import org.mozilla.fenix.mvi.UIView

class BookmarkUIView(
    container: ViewGroup,
    actionEmitter: Observer<BookmarkAction>,
    changesObservable: Observable<BookmarkChange>
) :
    UIView<BookmarkState, BookmarkAction, BookmarkChange>(container, actionEmitter, changesObservable),
    BackHandler {

    var mode: BookmarkState.Mode = BookmarkState.Mode.Normal
        private set
    var tree: BookmarkNode? = null
        private set

    private var canGoBack = false

    override val view: LinearLayout = LayoutInflater.from(container.context)
        .inflate(R.layout.component_bookmark, container, true) as LinearLayout

    private val bookmarkAdapter: BookmarkAdapter
    private val context = container.context
    private val activity = context?.asActivity()

    init {
        view.bookmark_list.apply {
            bookmarkAdapter = BookmarkAdapter(view.bookmarks_empty_view, actionEmitter)
            adapter = bookmarkAdapter
        }
    }

    override fun updateView() = Consumer<BookmarkState> {
        canGoBack = !(listOf(null, BookmarkRoot.Root.id).contains(it.tree?.guid))
        if (it.tree != tree) {
            tree = it.tree
        }
        if (it.mode != mode) {
            mode = it.mode
            actionEmitter.onNext(BookmarkAction.SwitchMode)
        }
        when (val modeCopy = it.mode) {
            is BookmarkState.Mode.Normal -> setUIForNormalMode(it.tree)
            is BookmarkState.Mode.Selecting -> setUIForSelectingMode(it.tree, modeCopy)
        }
    }

    override fun onBackPressed(): Boolean {
        return when {
            mode is BookmarkState.Mode.Selecting -> {
                actionEmitter.onNext(BookmarkAction.DeselectAll)
                true
            }
            canGoBack -> {
                actionEmitter.onNext(BookmarkAction.BackPressed)
                true
            }
            else -> false
        }
    }

    fun getSelected(): Set<BookmarkNode> = bookmarkAdapter.selected

    private fun setToolbarColors(foreground: Int, background: Int) {
        val toolbar = activity?.findViewById<Toolbar>(R.id.navigationToolbar)
        val colorFilter = PorterDuffColorFilter(
            ContextCompat.getColor(context, foreground), SRC_IN
        )
        toolbar?.run {
            setBackgroundColor(ContextCompat.getColor(context, background))
            setTitleTextColor(ContextCompat.getColor(context, foreground))
            themeToolbar(
                toolbar, foreground,
                background, colorFilter
            )
        }
    }

    private fun setUIForSelectingMode(
        root: BookmarkNode?,
        mode: BookmarkState.Mode.Selecting
    ) {
        bookmarkAdapter.updateData(root, mode)
        activity?.title =
            context.getString(R.string.bookmarks_multi_select_title, mode.selectedItems.size)
        setToolbarColors(
            R.color.white_color,
            R.attr.accentHighContrast.getColorIntFromAttr(context!!)
        )
    }

    private fun setUIForNormalMode(root: BookmarkNode?) {
        bookmarkAdapter.updateData(root, BookmarkState.Mode.Normal)
        setTitle(root)
        setToolbarColors(
            R.attr.primaryText.getColorIntFromAttr(context!!),
            R.attr.foundation.getColorIntFromAttr(context)
        )
    }

    private fun setTitle(root: BookmarkNode?) {
        (activity as? AppCompatActivity)?.title =
            if (root?.guid in setOf(
                    BookmarkRoot.Mobile.id,
                    null
                )
            ) {
                context.getString(R.string.library_bookmarks)
            } else {
                root!!.title
            }
    }

    private fun themeToolbar(
        toolbar: Toolbar,
        textColor: Int,
        backgroundColor: Int,
        colorFilter: PorterDuffColorFilter? = null
    ) {
        toolbar.setTitleTextColor(ContextCompat.getColor(context!!, textColor))
        toolbar.setBackgroundColor(ContextCompat.getColor(context, backgroundColor))

        if (colorFilter == null) {
            return
        }

        toolbar.overflowIcon?.colorFilter = colorFilter
        (0 until toolbar.childCount).forEach {
            when (val item = toolbar.getChildAt(it)) {
                is ImageButton -> item.drawable.colorFilter = colorFilter
                is ActionMenuView -> themeActionMenuView(item, colorFilter)
            }
        }
    }

    private fun themeActionMenuView(
        item: ActionMenuView,
        colorFilter: PorterDuffColorFilter
    ) {
        (0 until item.childCount).forEach {
            val innerChild = item.getChildAt(it)
            if (innerChild is ActionMenuItemView) {
                themeChildren(innerChild, item, colorFilter)
            }
        }
    }

    private fun themeChildren(
        innerChild: ActionMenuItemView,
        item: ActionMenuView,
        colorFilter: PorterDuffColorFilter
    ) {
        val drawables = innerChild.compoundDrawables
        for (k in drawables.indices) {
            drawables[k]?.let {
                item.post { innerChild.compoundDrawables[k].colorFilter = colorFilter }
            }
        }
    }
}

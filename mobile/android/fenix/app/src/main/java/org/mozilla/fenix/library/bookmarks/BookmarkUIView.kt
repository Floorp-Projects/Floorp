/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.LinearLayout
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_bookmark.view.*
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.support.base.feature.BackHandler
import org.mozilla.fenix.R
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

    var canGoBack = false

    override val view: LinearLayout = LayoutInflater.from(container.context)
        .inflate(R.layout.component_bookmark, container, true) as LinearLayout

    private val bookmarkAdapter: BookmarkAdapter

    init {
        view.bookmark_list.apply {
            bookmarkAdapter = BookmarkAdapter(view.bookmarks_empty_view, actionEmitter)
            adapter = bookmarkAdapter
        }
    }

    override fun updateView() = Consumer<BookmarkState> {
        canGoBack = !(listOf(null, BookmarkRoot.Root.id).contains(it.tree?.guid))
        bookmarkAdapter.updateData(it.tree, it.mode)
        if (it.mode != mode) {
            mode = it.mode
            actionEmitter.onNext(BookmarkAction.ModeChanged)
        }
    }

    override fun onBackPressed(): Boolean {
        return if (canGoBack) {
            actionEmitter.onNext(BookmarkAction.BackPressed)
            true
        } else false
    }

    fun getSelected(): Set<BookmarkNode> = bookmarkAdapter.selected
}

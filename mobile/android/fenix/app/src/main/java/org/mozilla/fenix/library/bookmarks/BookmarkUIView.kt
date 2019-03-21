/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import mozilla.appservices.places.BookmarkRoot
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

    override val view: RecyclerView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_bookmark, container, true)
        .findViewById(R.id.bookmark_list)

    private val bookmarkAdapter = BookmarkAdapter(actionEmitter)

    init {
        view.apply {
            adapter = bookmarkAdapter
            layoutManager = LinearLayoutManager(container.context)
        }
    }

    override fun updateView() = Consumer<BookmarkState> {
        canGoBack = !(listOf(null, BookmarkRoot.Root.id).contains(it.tree?.guid))
        bookmarkAdapter.updateData(it.tree, it.mode)
        mode = it.mode
    }

    override fun onBackPressed(): Boolean {
        return if (canGoBack) {
            actionEmitter.onNext(BookmarkAction.BackPressed)
            true
        } else false
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import io.mockk.MockKAnnotations
import io.reactivex.Observer
import io.reactivex.observers.TestObserver
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.TestUtils
import org.mozilla.fenix.TestUtils.bus
import org.mozilla.fenix.ext.minus
import org.mozilla.fenix.mvi.getManagedEmitter

class BookmarkViewModelTest {

    private lateinit var bookmarkViewModel: BookmarkViewModel
    private lateinit var bookmarkObserver: TestObserver<BookmarkState>
    private lateinit var emitter: Observer<BookmarkChange>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        TestUtils.setRxSchedulers()

        bookmarkViewModel = BookmarkViewModel.create()
        bookmarkObserver = bookmarkViewModel.state.test()
        bus.getSafeManagedObservable(BookmarkChange::class.java)
            .subscribe(bookmarkViewModel.changes::onNext)
        emitter = TestUtils.owner.getManagedEmitter()
    }

    @Test
    fun `select and deselect a bookmark`() {
        val itemToSelect = BookmarkNode(BookmarkNodeType.ITEM, "234", "123", 0, "Mozilla", "http://mozilla.org", null)
        val separator = BookmarkNode(BookmarkNodeType.SEPARATOR, "345", "123", 1, null, null, null)
        val innerFolder = BookmarkNode(BookmarkNodeType.FOLDER, "456", "123", 2, "Web Browsers", null, null)
        val tree = BookmarkNode(
            BookmarkNodeType.FOLDER, "123", BookmarkRoot.Mobile.id, 0, "Best Sites", null,
            listOf(itemToSelect, separator, innerFolder)
        )

        emitter.onNext(BookmarkChange.Change(tree))
        emitter.onNext(BookmarkChange.IsSelected(itemToSelect))
        emitter.onNext(BookmarkChange.IsDeselected(itemToSelect))

        bookmarkObserver.assertSubscribed().awaitCount(2).assertNoErrors()
            .assertValues(
                BookmarkState(null, BookmarkState.Mode.Normal),
                BookmarkState(tree, BookmarkState.Mode.Normal),
                BookmarkState(tree, BookmarkState.Mode.Selecting(setOf(itemToSelect))),
                BookmarkState(tree, BookmarkState.Mode.Normal)
            )
    }

    @Test
    fun `select and delete a bookmark`() {
        val itemToSelect = BookmarkNode(BookmarkNodeType.ITEM, "234", "123", 0, "Mozilla", "http://mozilla.org", null)
        val separator = BookmarkNode(BookmarkNodeType.SEPARATOR, "345", "123", 1, null, null, null)
        val innerFolder = BookmarkNode(BookmarkNodeType.FOLDER, "456", "123", 2, "Web Browsers", null, null)
        val tree = BookmarkNode(
            BookmarkNodeType.FOLDER, "123", BookmarkRoot.Mobile.id, 0, "Best Sites", null,
            listOf(itemToSelect, separator, innerFolder)
        )

        emitter.onNext(BookmarkChange.Change(tree))
        emitter.onNext(BookmarkChange.IsSelected(itemToSelect))
        emitter.onNext(BookmarkChange.Change(tree - itemToSelect.guid))

        bookmarkObserver.assertSubscribed().awaitCount(2).assertNoErrors()
            .assertValues(
                BookmarkState(null, BookmarkState.Mode.Normal),
                BookmarkState(tree, BookmarkState.Mode.Normal),
                BookmarkState(tree, BookmarkState.Mode.Selecting(setOf(itemToSelect))),
                BookmarkState(tree - itemToSelect.guid, BookmarkState.Mode.Normal)
            )
    }
}

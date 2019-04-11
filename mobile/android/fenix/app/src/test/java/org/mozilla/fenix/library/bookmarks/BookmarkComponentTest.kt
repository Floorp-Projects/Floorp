/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.view.ViewGroup
import io.mockk.MockKAnnotations
import io.mockk.mockk
import io.mockk.spyk
import io.reactivex.Observer
import io.reactivex.observers.TestObserver
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test
import org.mozilla.fenix.TestUtils
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.UIView
import org.mozilla.fenix.mvi.getManagedEmitter

class BookmarkComponentTest {

    private lateinit var bookmarkComponent: BookmarkComponentTest.TestBookmarkComponent
    private lateinit var bookmarkObserver: TestObserver<BookmarkState>
    private lateinit var emitter: Observer<BookmarkChange>

    @BeforeEach
    fun setup() {
        MockKAnnotations.init(this)
        TestUtils.setRxSchedulers()

        bookmarkComponent = spyk(
            BookmarkComponentTest.TestBookmarkComponent(mockk(), TestUtils.bus),
            recordPrivateCalls = true
        )
        bookmarkObserver = bookmarkComponent.internalRender(bookmarkComponent.reducer).test()
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

    @Suppress("MemberVisibilityCanBePrivate")
    class TestBookmarkComponent(container: ViewGroup, bus: ActionBusFactory) :
        BookmarkComponent(container, bus) {

        override val uiView: UIView<BookmarkState, BookmarkAction, BookmarkChange>
            get() = mockk(relaxed = true)
    }
}

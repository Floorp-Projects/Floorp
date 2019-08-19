/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import io.mockk.called
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowsingMode
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

@SuppressWarnings("TooManyFunctions", "LargeClass")
class BookmarkFragmentInteractorTest {

    private lateinit var interactor: BookmarkFragmentInteractor

    private val bookmarkStore = spyk(BookmarkStore(BookmarkState(null)))
    private val sharedViewModel: BookmarksSharedViewModel = mockk(relaxed = true)
    private val bookmarkController: DefaultBookmarkController = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)

    private val item = BookmarkNode(BookmarkNodeType.ITEM, "456", "123", 0, "Mozilla", "http://mozilla.org", null)
    private val separator = BookmarkNode(BookmarkNodeType.SEPARATOR, "789", "123", 1, null, null, null)
    private val subfolder = BookmarkNode(BookmarkNodeType.FOLDER, "987", "123", 0, "Subfolder", null, listOf())
    private val tree: BookmarkNode = BookmarkNode(
        BookmarkNodeType.FOLDER, "123", null, 0, "Mobile", null, listOf(item, separator, item, subfolder)
    )
    private val root = BookmarkNode(
        BookmarkNodeType.FOLDER, BookmarkRoot.Root.id, null, 0, BookmarkRoot.Root.name, null, null
    )

    @Before
    fun setup() {
        every { bookmarkStore.dispatch(any()) } returns mockk()

        interactor =
            BookmarkFragmentInteractor(
                bookmarkStore = bookmarkStore,
                viewModel = sharedViewModel,
                bookmarksController = bookmarkController,
                metrics = metrics
            )
    }

    @Test
    fun `update bookmarks tree`() {
        interactor.onBookmarksChanged(tree)

        verify {
            bookmarkStore.dispatch(BookmarkAction.Change(tree))
        }
    }

    @Test
    fun `open a bookmark item`() {
        interactor.open(item)

        verifyOrder {
            bookmarkController.handleBookmarkTapped(item)
            metrics.track(Event.OpenedBookmark)
        }
    }

    @Test(expected = IllegalStateException::class)
    fun `open a separator`() {
        interactor.open(item.copy(type = BookmarkNodeType.SEPARATOR))
    }

    @Test
    fun `expand a level of bookmarks`() {
        interactor.open(tree)

        verify {
            bookmarkController.handleBookmarkExpand(tree)
        }
    }

    @Test
    fun `switch between bookmark selection modes`() {
        interactor.onSelectionModeSwitch(BookmarkState.Mode.Normal)

        verify {
            bookmarkController.handleSelectionModeSwitch()
        }
    }

    @Test
    fun `press the edit bookmark button`() {
        interactor.onEditPressed(item)

        verify {
            bookmarkController.handleBookmarkEdit(item)
        }
    }

    @Test
    fun `select a bookmark item`() {
        interactor.select(item)

        verify {
            bookmarkStore.dispatch(BookmarkAction.Select(item))
        }
    }

    @Test
    fun `deselect a bookmark item`() {
        interactor.deselect(item)

        verify {
            bookmarkStore.dispatch(BookmarkAction.Deselect(item))
        }
    }

    @Test
    fun `deselectAll bookmark items`() {
        interactor.onAllBookmarksDeselected()

        verify {
            bookmarkStore.dispatch(BookmarkAction.DeselectAll)
        }
    }

    @Test
    fun `cannot select bookmark roots`() {
        interactor.select(root)

        verify { bookmarkStore wasNot called }
    }

    @Test
    fun `copy a bookmark item`() {
        interactor.onCopyPressed(item)

        verifyOrder {
            bookmarkController.handleCopyUrl(item)
            metrics.track(Event.CopyBookmark)
        }
    }

    @Test
    fun `share a bookmark item`() {
        interactor.onSharePressed(item)

        verifyOrder {
            bookmarkController.handleBookmarkSharing(item)
            metrics.track(Event.ShareBookmark)
        }
    }

    @Test
    fun `open a bookmark item in a new tab`() {
        interactor.onOpenInNormalTab(item)

        verifyOrder {
            bookmarkController.handleOpeningBookmark(item, BrowsingMode.Normal)
            metrics.track(Event.OpenedBookmarkInNewTab)
        }
    }

    @Test
    fun `open a bookmark item in a private tab`() {
        interactor.onOpenInPrivateTab(item)

        verifyOrder {
            bookmarkController.handleOpeningBookmark(item, BrowsingMode.Private)
            metrics.track(Event.OpenedBookmarkInPrivateTab)
        }
    }

    @Test
    fun `delete a bookmark item`() {
        interactor.onDelete(setOf(item))

        verify {
            bookmarkController.handleBookmarkDeletion(setOf(item), Event.RemoveBookmark)
        }
    }

    @Test(expected = IllegalStateException::class)
    fun `delete a separator`() {
        interactor.onDelete(setOf(item, item.copy(type = BookmarkNodeType.SEPARATOR)))
    }

    @Test
    fun `delete a bookmark folder`() {
        interactor.onDelete(setOf(subfolder))

        verify {
            bookmarkController.handleBookmarkDeletion(setOf(subfolder), Event.RemoveBookmarkFolder)
        }
    }

    @Test
    fun `delete multiple bookmarks`() {
        interactor.onDelete(setOf(item, subfolder))

        verify {
            bookmarkController.handleBookmarkDeletion(setOf(item, subfolder), Event.RemoveBookmarks)
        }
    }

    @Test
    fun `press the back button`() {
        interactor.onBackPressed()

        verify {
            bookmarkController.handleBackPressed()
        }
    }

    @Test
    fun `clicked sign in on bookmarks screen`() {
        interactor.onSignInPressed()

        verify {
            bookmarkController.handleSigningIn()
        }
    }

    @Test
    fun `got signed in signal on bookmarks screen`() {
        interactor.onSignedIn()

        verify {
            sharedViewModel.signedIn.postValue(true)
        }
    }

    @Test
    fun `got signed out signal on bookmarks screen`() {
        interactor.onSignedOut()

        verify {
            sharedViewModel.signedIn.postValue(false)
        }
    }
}

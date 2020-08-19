/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.ClipData
import android.content.ClipboardManager
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.MockKAnnotations
import io.mockk.called
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.metrics.Event

@ExperimentalCoroutinesApi
class BookmarkControllerTest {

    private val scope = TestCoroutineScope()

    @MockK private lateinit var bookmarkStore: BookmarkFragmentStore
    @MockK private lateinit var sharedViewModel: BookmarksSharedViewModel
    @MockK(relaxUnitFun = true) private lateinit var clipboardManager: ClipboardManager
    @MockK(relaxed = true) private lateinit var homeActivity: HomeActivity
    @MockK(relaxed = true) private lateinit var bookmarkStorage: BookmarksStorage
    @MockK(relaxed = true) private lateinit var accountManager: FxaAccountManager
    @MockK(relaxed = true) private lateinit var navController: NavController
    @MockK(relaxed = true) private lateinit var loadBookmarkNode: suspend (String) -> BookmarkNode?
    @MockK(relaxed = true) private lateinit var showSnackbar: (String) -> Unit
    @MockK(relaxed = true) private lateinit var deleteBookmarkNodes: (Set<BookmarkNode>, Event) -> Unit
    @MockK(relaxed = true) private lateinit var deleteBookmarkFolder: (Set<BookmarkNode>) -> Unit
    @MockK(relaxed = true) private lateinit var invokePendingDeletion: () -> Unit

    private lateinit var controller: BookmarkController

    private val item =
        BookmarkNode(BookmarkNodeType.ITEM, "456", "123", 0, "Mozilla", "http://mozilla.org", null)
    private val subfolder =
        BookmarkNode(BookmarkNodeType.FOLDER, "987", "123", 0, "Subfolder", null, listOf())
    private val childItem = BookmarkNode(
        BookmarkNodeType.ITEM,
        "987",
        "123",
        2,
        "Firefox",
        "https://www.mozilla.org/en-US/firefox/",
        null
    )
    private val tree = BookmarkNode(
        BookmarkNodeType.FOLDER,
        "123",
        null,
        0,
        "Mobile",
        null,
        listOf(item, item, childItem, subfolder)
    )
    private val root = BookmarkNode(
        BookmarkNodeType.FOLDER, BookmarkRoot.Root.id, null, 0, BookmarkRoot.Root.name, null, null
    )

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        loadBookmarkNode = mockk(relaxed = true)

        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.bookmarkFragment
        }
        every { bookmarkStore.state } returns BookmarkFragmentState(null)
        every { bookmarkStore.dispatch(any()) } returns mockk()
        every { sharedViewModel.selectedFolder = any() } just runs

        controller = DefaultBookmarkController(
            activity = homeActivity,
            bookmarkStorage = bookmarkStorage,
            accountManager = accountManager,
            navController = navController,
            clipboardManager = clipboardManager,
            scope = scope,
            store = bookmarkStore,
            sharedViewModel = sharedViewModel,
            loadBookmarkNode = loadBookmarkNode,
            showSnackbar = showSnackbar,
            deleteBookmarkNodes = deleteBookmarkNodes,
            deleteBookmarkFolder = deleteBookmarkFolder,
            invokePendingDeletion = invokePendingDeletion
        )
    }

    @After
    fun cleanUp() {
        scope.cleanupTestCoroutines()
    }

    @Test
    fun `handleBookmarkChanged updates the selected bookmark node`() {
        controller.handleBookmarkChanged(tree)

        verify {
            sharedViewModel.selectedFolder = tree
            bookmarkStore.dispatch(BookmarkFragmentAction.Change(tree))
        }
    }

    @Test
    fun `handleBookmarkTapped should load the bookmark in a new tab`() {
        controller.handleBookmarkTapped(item)

        verifyOrder {
            invokePendingDeletion.invoke()
            homeActivity.openToBrowserAndLoad(item.url!!, true, BrowserDirection.FromBookmarks)
        }
    }

    @Test
    fun `handleBookmarkTapped should respect browsing mode`() {
        // if in normal mode, should be in normal mode
        every { homeActivity.browsingModeManager.mode } returns BrowsingMode.Normal

        controller.handleBookmarkTapped(item)
        assertEquals(BrowsingMode.Normal, homeActivity.browsingModeManager.mode)

        // if in private mode, should be in private mode
        every { homeActivity.browsingModeManager.mode } returns BrowsingMode.Private

        controller.handleBookmarkTapped(item)
        assertEquals(BrowsingMode.Private, homeActivity.browsingModeManager.mode)
    }

    @Test
    fun `handleBookmarkExpand clears selection and invokes pending deletions`() {
        coEvery { loadBookmarkNode.invoke(any()) } returns tree

        controller.handleBookmarkExpand(tree)

        verify {
            invokePendingDeletion.invoke()
            controller.handleAllBookmarksDeselected()
        }
    }

    @Test
    fun `handleBookmarkExpand should refresh and change the active bookmark node`() {
        coEvery { loadBookmarkNode.invoke(any()) } returns tree

        controller.handleBookmarkExpand(tree)

        coVerify {
            loadBookmarkNode.invoke(tree.guid)
            sharedViewModel.selectedFolder = tree
            bookmarkStore.dispatch(BookmarkFragmentAction.Change(tree))
        }
    }

    @Test
    fun `handleSelectionModeSwitch should invalidateOptionsMenu`() {
        controller.handleSelectionModeSwitch()

        verify {
            homeActivity.invalidateOptionsMenu()
        }
    }

    @Test
    fun `handleBookmarkEdit should navigate to the 'Edit' fragment`() {
        controller.handleBookmarkEdit(item)

        verify {
            invokePendingDeletion.invoke()
            navController.navigate(
                BookmarkFragmentDirections.actionBookmarkFragmentToBookmarkEditFragment(
                    item.guid
                ),
                null
            )
        }
    }

    @Test
    fun `handleBookmarkSelected dispatches Select action when selecting a non-root folder`() {
        controller.handleBookmarkSelected(item)

        verify {
            bookmarkStore.dispatch(BookmarkFragmentAction.Select(item))
        }
    }

    @Test
    fun `handleBookmarkSelected should show a toast when selecting a root folder`() {
        every { homeActivity.resources.getString(R.string.bookmark_cannot_edit_root) } returns "Can't edit default folders"

        controller.handleBookmarkSelected(root)

        verify {
            showSnackbar("Can't edit default folders")
        }
    }

    @Test
    fun `handleBookmarkSelected does not select in Syncing mode`() {
        every { bookmarkStore.state.mode } returns BookmarkFragmentState.Mode.Syncing

        controller.handleBookmarkSelected(item)

        verify { bookmarkStore.dispatch(BookmarkFragmentAction.Select(item)) wasNot called }
    }

    @Test
    fun `handleBookmarkDeselected dispatches Deselect action`() {
        controller.handleBookmarkDeselected(item)

        verify {
            bookmarkStore.dispatch(BookmarkFragmentAction.Deselect(item))
        }
    }

    @Test
    fun `handleCopyUrl should copy bookmark url to clipboard and show a toast`() {
        every { homeActivity.resources.getString(R.string.url_copied) } returns "URL copied"

        controller.handleCopyUrl(item)

        verifyOrder {
            ClipData.newPlainText(item.url, item.url)
            showSnackbar("URL copied")
        }
    }

    @Test
    fun `handleBookmarkSharing should navigate to the 'Share' fragment`() {
        controller.handleBookmarkSharing(item)

        verify {
            navController.navigate(withArg<NavDirections> {
                assertEquals(R.id.action_global_shareFragment, it.actionId)
            }, null)
        }
    }

    @Test
    fun `handleOpeningBookmark should open the bookmark a new 'Normal' tab`() {
        controller.handleOpeningBookmark(item, BrowsingMode.Normal)

        verifyOrder {
            invokePendingDeletion.invoke()
            homeActivity.browsingModeManager.mode = BrowsingMode.Normal
            homeActivity.openToBrowserAndLoad(item.url!!, true, BrowserDirection.FromBookmarks)
        }
    }

    @Test
    fun `handleOpeningBookmark should open the bookmark a new 'Private' tab`() {
        controller.handleOpeningBookmark(item, BrowsingMode.Private)

        verifyOrder {
            invokePendingDeletion.invoke()
            homeActivity.browsingModeManager.mode = BrowsingMode.Private
            homeActivity.openToBrowserAndLoad(item.url!!, true, BrowserDirection.FromBookmarks)
        }
    }

    @Test
    fun `handleBookmarkDeletion for an item should properly call a passed in delegate`() {
        controller.handleBookmarkDeletion(setOf(item), Event.RemoveBookmark)

        verify {
            deleteBookmarkNodes(setOf(item), Event.RemoveBookmark)
        }
    }

    @Test
    fun `handleBookmarkDeletion for multiple bookmarks should properly call a passed in delegate`() {
        controller.handleBookmarkDeletion(setOf(item, subfolder), Event.RemoveBookmarks)

        verify {
            deleteBookmarkNodes(setOf(item, subfolder), Event.RemoveBookmarks)
        }
    }

    @Test
    fun `handleBookmarkDeletion for a folder should properly call the delete folder delegate`() {
        controller.handleBookmarkFolderDeletion(setOf(subfolder))

        verify {
            deleteBookmarkFolder(setOf(subfolder))
        }
    }

    @Test
    fun `handleRequestSync dispatches actions in the correct order`() {
        coEvery { bookmarkStorage.getBookmark(any()) } returns tree
        coEvery { loadBookmarkNode.invoke(any()) } returns tree

        controller.handleRequestSync()

        verifyOrder {
            bookmarkStore.dispatch(BookmarkFragmentAction.StartSync)
            bookmarkStore.dispatch(BookmarkFragmentAction.FinishSync)
        }
    }

    @Test
    fun `handleBackPressed with one item in backstack should trigger handleBackPressed in NavController`() {
        every { bookmarkStore.state.guidBackstack } returns listOf(tree.guid)
        every { bookmarkStore.state.tree } returns tree

        controller.handleBackPressed()

        verify {
            invokePendingDeletion.invoke()
            navController.popBackStack()
        }
    }

    @Test
    fun `handleStartSwipingItem disables swipe to refresh`() {
        controller.handleStartSwipingItem()

        verify {
            bookmarkStore.dispatch(BookmarkFragmentAction.SwipeRefreshAvailabilityChanged(false))
        }
    }

    @Test
    fun `handleStopSwipingItem attempts to enable swipe to refresh`() {
        controller.handleStopSwipingItem()

        verify {
            bookmarkStore.dispatch(BookmarkFragmentAction.SwipeRefreshAvailabilityChanged(true))
        }
    }
}

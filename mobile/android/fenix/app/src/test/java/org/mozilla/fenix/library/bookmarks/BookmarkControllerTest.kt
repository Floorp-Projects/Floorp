/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import io.mockk.Runs
import io.mockk.called
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Services
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components

@Suppress("TooManyFunctions", "LargeClass")
@ExperimentalCoroutinesApi
class BookmarkControllerTest {

    private lateinit var controller: BookmarkController

    private val bookmarkStore = spyk(BookmarkFragmentStore(BookmarkFragmentState(null)))
    private val context: Context = mockk(relaxed = true)
    private val scope = TestCoroutineScope()
    private val clipboardManager: ClipboardManager = mockk(relaxUnitFun = true)
    private val navController: NavController = mockk(relaxed = true)
    private val sharedViewModel: BookmarksSharedViewModel = mockk()
    private val tabsUseCases: TabsUseCases = mockk()
    private val loadBookmarkNode: suspend (String) -> BookmarkNode? = mockk(relaxed = true)
    private val showSnackbar: (String) -> Unit = mockk(relaxed = true)
    private val deleteBookmarkNodes: (Set<BookmarkNode>, Event) -> Unit = mockk(relaxed = true)
    private val deleteBookmarkFolder: (Set<BookmarkNode>) -> Unit = mockk(relaxed = true)
    private val invokePendingDeletion: () -> Unit = mockk(relaxed = true)
    private val showTabTray: () -> Unit = mockk(relaxed = true)

    private val homeActivity: HomeActivity = mockk(relaxed = true)
    private val services: Services = mockk(relaxed = true)
    private val addNewTabUseCase: TabsUseCases.AddNewTabUseCase = mockk(relaxed = true)

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
        every { homeActivity.components.services } returns services
        every { navController.currentDestination } returns NavDestination("").apply {
            id = R.id.bookmarkFragment
        }
        every { bookmarkStore.dispatch(any()) } returns mockk()
        every { sharedViewModel.selectedFolder = any() } just runs
        every { tabsUseCases.addTab } returns addNewTabUseCase

        controller = DefaultBookmarkController(
            activity = homeActivity,
            navController = navController,
            clipboardManager = clipboardManager,
            scope = scope,
            store = bookmarkStore,
            sharedViewModel = sharedViewModel,
            tabsUseCases = tabsUseCases,
            loadBookmarkNode = loadBookmarkNode,
            showSnackbar = showSnackbar,
            deleteBookmarkNodes = deleteBookmarkNodes,
            deleteBookmarkFolder = deleteBookmarkFolder,
            invokePendingDeletion = invokePendingDeletion,
            showTabTray = showTabTray
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
        val errorMessage = context.getString(R.string.bookmark_cannot_edit_root)

        controller.handleBookmarkSelected(root)

        verify {
            showSnackbar(errorMessage)
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
        val urlCopiedMessage = context.getString(R.string.url_copied)

        controller.handleCopyUrl(item)

        verifyOrder {
            ClipData.newPlainText(item.url, item.url)
            showSnackbar(urlCopiedMessage)
        }
    }

    @Test
    fun `handleBookmarkSharing should navigate to the 'Share' fragment`() {
        val navDirectionsSlot = slot<NavDirections>()
        every { navController.navigate(capture(navDirectionsSlot), null) } just Runs

        controller.handleBookmarkSharing(item)

        verify {
            navController.navigate(navDirectionsSlot.captured, null)
        }
    }

    @Test
    fun `handleBookmarkTapped should open the bookmark`() {
        controller.handleBookmarkTapped(item)

        verifyOrder {
            invokePendingDeletion.invoke()
            homeActivity.openToBrowserAndLoad(item.url!!, true, BrowserDirection.FromBookmarks)
        }
    }

    @Test
    fun `handleOpeningBookmark should open the bookmark a new 'Normal' tab`() {
        controller.handleOpeningBookmark(item, BrowsingMode.Normal)

        verifyOrder {
            invokePendingDeletion.invoke()
            homeActivity.browsingModeManager.mode = BrowsingMode.Normal
            addNewTabUseCase.invoke(item.url!!, private = false)
            showTabTray
        }
    }

    @Test
    fun `handleOpeningBookmark should open the bookmark a new 'Private' tab`() {
        controller.handleOpeningBookmark(item, BrowsingMode.Private)

        verifyOrder {
            invokePendingDeletion.invoke()
            homeActivity.browsingModeManager.mode = BrowsingMode.Private
            addNewTabUseCase.invoke(item.url!!, private = true)
            showTabTray
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
        every { homeActivity.components.backgroundServices.accountManager } returns mockk(relaxed = true)
        coEvery { homeActivity.bookmarkStorage.getBookmark(any()) } returns tree
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
}

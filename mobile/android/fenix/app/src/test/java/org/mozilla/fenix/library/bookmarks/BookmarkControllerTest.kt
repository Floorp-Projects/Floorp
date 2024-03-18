/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.ClipboardManager
import android.content.Context
import androidx.navigation.NavBackStackEntry
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
import io.mockk.mockkConstructor
import io.mockk.runs
import io.mockk.slot
import io.mockk.spyk
import io.mockk.unmockkConstructor
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Services
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components

@Suppress("TooManyFunctions", "LargeClass")
class BookmarkControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val bookmarkStore = spyk(BookmarkFragmentStore(BookmarkFragmentState(null)))
    private val context: Context = mockk(relaxed = true)
    private val clipboardManager: ClipboardManager = mockk(relaxUnitFun = true)
    private val navController: NavController = mockk(relaxed = true)
    private val sharedViewModel: BookmarksSharedViewModel = mockk()
    private val tabsUseCases: TabsUseCases = mockk()
    private val homeActivity: HomeActivity = mockk(relaxed = true)
    private val services: Services = mockk(relaxed = true)
    private val addNewTabUseCase: TabsUseCases.AddNewTabUseCase = mockk(relaxed = true)
    private val navBackStackEntry: NavBackStackEntry = mockk(relaxed = true)
    private val navDestination: NavDestination = mockk(relaxed = true)

    private val item =
        BookmarkNode(BookmarkNodeType.ITEM, "456", "123", 0u, "Mozilla", "http://mozilla.org", 0, null)
    private val subfolder =
        BookmarkNode(BookmarkNodeType.FOLDER, "987", "123", 0u, "Subfolder", null, 0, listOf())
    private val childItem = BookmarkNode(
        BookmarkNodeType.ITEM,
        "987",
        "123",
        2u,
        "Firefox",
        "https://www.mozilla.org/en-US/firefox/",
        0,
        null,
    )
    private val tree = BookmarkNode(
        BookmarkNodeType.FOLDER,
        "123",
        null,
        0u,
        "Mobile",
        null,
        0,
        listOf(item, item, childItem, subfolder),
    )
    private val largeTree = BookmarkNode(
        BookmarkNodeType.FOLDER,
        "123",
        null,
        0u,
        "Mobile",
        null,
        0,
        List(WARN_OPEN_ALL_SIZE) { item },
    )
    private val root = BookmarkNode(
        BookmarkNodeType.FOLDER,
        BookmarkRoot.Root.id,
        null,
        0u,
        BookmarkRoot.Root.name,
        null,
        0,
        null,
    )

    @Before
    fun setup() {
        every { homeActivity.components.services } returns services
        every { navController.currentDestination } returns NavDestination("").apply {
            id = R.id.bookmarkFragment
        }
        every { navController.previousBackStackEntry } returns navBackStackEntry
        every { navBackStackEntry.destination } returns navDestination
        every { navDestination.id } returns R.id.browserFragment
        every { bookmarkStore.dispatch(any()) } returns mockk()
        every { sharedViewModel.selectedFolder = any() } just runs
        every { tabsUseCases.addTab } returns addNewTabUseCase
    }

    @Test
    fun `handleBookmarkChanged updates the selected bookmark node`() {
        createController().handleBookmarkChanged(tree)

        verify {
            sharedViewModel.selectedFolder = tree
            bookmarkStore.dispatch(BookmarkFragmentAction.Change(tree))
        }
    }

    @Test
    fun `WHEN handleBookmarkTapped is called with BrowserFragment THEN load the bookmark in current tab`() {
        val flags = EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.ALLOW_JAVASCRIPT_URL)

        createController().handleBookmarkTapped(item)

        verify {
            homeActivity.openToBrowserAndLoad(
                item.url!!,
                false,
                BrowserDirection.FromBookmarks,
                flags = flags,
            )
        }
    }

    @Test
    fun `WHEN handleBookmarkTapped is called with HomeFragment THEN load the bookmark in new tab`() {
        val flags = EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.ALLOW_JAVASCRIPT_URL)

        every { navDestination.id } returns R.id.homeFragment

        createController().handleBookmarkTapped(item)

        verify {
            homeActivity.openToBrowserAndLoad(
                item.url!!,
                true,
                BrowserDirection.FromBookmarks,
                flags = flags,
            )
        }
    }

    @Test
    fun `WHEN handleBookmarkTapped is called with private browsing THEN load the bookmark in new tab`() {
        every { homeActivity.browsingModeManager.mode } returns BrowsingMode.Private
        val flags = EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.ALLOW_JAVASCRIPT_URL)

        createController().handleBookmarkTapped(item)

        verify {
            homeActivity.openToBrowserAndLoad(
                item.url!!,
                true,
                BrowserDirection.FromBookmarks,
                flags = flags,
            )
        }
    }

    @Test
    fun `handleBookmarkTapped should respect browsing mode`() {
        // if in normal mode, should be in normal mode
        every { homeActivity.browsingModeManager.mode } returns BrowsingMode.Normal

        val controller = createController()
        controller.handleBookmarkTapped(item)
        assertEquals(BrowsingMode.Normal, homeActivity.browsingModeManager.mode)

        // if in private mode, should be in private mode
        every { homeActivity.browsingModeManager.mode } returns BrowsingMode.Private

        controller.handleBookmarkTapped(item)
        assertEquals(BrowsingMode.Private, homeActivity.browsingModeManager.mode)
    }

    @Test
    fun `handleBookmarkExpand should refresh and change the active bookmark node`() = runTestOnMain {
        var loadBookmarkNodeInvoked = false
        createController(
            loadBookmarkNode = { _: String, _: Boolean ->
                loadBookmarkNodeInvoked = true
                tree
            },
        ).handleBookmarkExpand(tree)

        assertTrue(loadBookmarkNodeInvoked)
        coVerify {
            sharedViewModel.selectedFolder = tree
            bookmarkStore.dispatch(BookmarkFragmentAction.Change(tree))
        }
    }

    @Test
    fun `handleSelectionModeSwitch should invalidateOptionsMenu`() {
        createController().handleSelectionModeSwitch()

        verify {
            homeActivity.invalidateOptionsMenu()
        }
    }

    @Test
    fun `handleBookmarkEdit should navigate to the 'Edit' fragment`() {
        createController().handleBookmarkEdit(item)

        verify {
            navController.navigate(
                BookmarkFragmentDirections.actionBookmarkFragmentToBookmarkEditFragment(
                    item.guid,
                ),
                null,
            )
        }
    }

    @Test
    fun `WHEN handling search THEN navigate to the search dialog fragment`() {
        createController().handleSearch()

        verify {
            navController.navigate(
                BookmarkFragmentDirections.actionGlobalSearchDialog(sessionId = null),
            )
        }
    }

    @Test
    fun `handleBookmarkSelected dispatches Select action when selecting a non-root folder`() {
        createController().handleBookmarkSelected(item)

        verify {
            bookmarkStore.dispatch(BookmarkFragmentAction.Select(item))
        }
    }

    @Test
    fun `handleBookmarkSelected should show a toast when selecting a root folder`() {
        val errorMessage = context.getString(R.string.bookmark_cannot_edit_root)

        var showSnackbarInvoked = false
        createController(
            showSnackbar = {
                assertEquals(errorMessage, it)
                showSnackbarInvoked = true
            },
        ).handleBookmarkSelected(root)

        assertTrue(showSnackbarInvoked)
    }

    @Test
    fun `handleBookmarkSelected does not select in Syncing mode`() {
        every { bookmarkStore.state.mode } returns BookmarkFragmentState.Mode.Syncing

        createController().handleBookmarkSelected(item)

        verify { bookmarkStore.dispatch(BookmarkFragmentAction.Select(item)) wasNot called }
    }

    @Test
    fun `handleBookmarkDeselected dispatches Deselect action`() {
        createController().handleBookmarkDeselected(item)

        verify {
            bookmarkStore.dispatch(BookmarkFragmentAction.Deselect(item))
        }
    }

    @Test
    fun `handleCopyUrl should copy bookmark url to clipboard and show a toast`() {
        val urlCopiedMessage = context.getString(R.string.url_copied)

        var showSnackbarInvoked = false
        createController(
            showSnackbar = {
                assertEquals(urlCopiedMessage, it)
                showSnackbarInvoked = true
            },
        ).handleCopyUrl(item)

        assertTrue(showSnackbarInvoked)
    }

    @Test
    fun `handleBookmarkSharing should navigate to the 'Share' fragment`() {
        val navDirectionsSlot = slot<NavDirections>()
        every { navController.navigate(capture(navDirectionsSlot), null) } just Runs

        createController().handleBookmarkSharing(item)

        verify {
            navController.navigate(navDirectionsSlot.captured, null)
        }
    }

    @Test
    fun `handleBookmarkTapped should open the bookmark`() {
        val flags =
            EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.ALLOW_JAVASCRIPT_URL)

        createController().handleBookmarkTapped(item)

        verify {
            homeActivity.openToBrowserAndLoad(
                item.url!!,
                false,
                BrowserDirection.FromBookmarks,
                flags = flags,
            )
        }
    }

    @Test
    fun `handleOpeningBookmark should open the bookmark a new 'Normal' tab`() {
        var showTabTrayInvoked = false
        var openedToPrivateTabsPage: Boolean? = null
        createController(
            showTabTray = { openToPrivateTabsPage ->
                openedToPrivateTabsPage = openToPrivateTabsPage
                showTabTrayInvoked = true
            },
        ).handleOpeningBookmark(item, BrowsingMode.Normal)

        assertTrue(showTabTrayInvoked)
        assertNotNull(openedToPrivateTabsPage)
        assertFalse(openedToPrivateTabsPage!!)
        verifyOrder {
            homeActivity.browsingModeManager.mode = BrowsingMode.Normal
            addNewTabUseCase.invoke(item.url!!, private = false)
        }
    }

    @Test
    fun `handleOpeningBookmark should open the bookmark a new 'Private' tab`() {
        var showTabTrayInvoked = false
        var openedToPrivateTabsPage: Boolean? = null
        createController(
            showTabTray = { openToPrivateTabsPage ->
                openedToPrivateTabsPage = openToPrivateTabsPage
                showTabTrayInvoked = true
            },
        ).handleOpeningBookmark(item, BrowsingMode.Private)

        assertTrue(showTabTrayInvoked)
        assertNotNull(openedToPrivateTabsPage)
        assertTrue(openedToPrivateTabsPage!!)
        verifyOrder {
            homeActivity.browsingModeManager.mode = BrowsingMode.Private
            addNewTabUseCase.invoke(item.url!!, private = true)
        }
    }

    @Test
    fun `WHEN handle opening folder bookmarks THEN all bookmarks in folder is opened in normal tabs`() {
        var showTabTrayInvoked = false
        var tabsTrayOpenedToPrivateTabs: Boolean? = null
        createController(
            showTabTray = { openToPrivateTabsPage ->
                tabsTrayOpenedToPrivateTabs = openToPrivateTabsPage
                showTabTrayInvoked = true
            },
            loadBookmarkNode = { guid: String, _: Boolean ->
                fun recurseFind(item: BookmarkNode, guid: String): BookmarkNode? {
                    if (item.guid == guid) {
                        return item
                    } else {
                        item.children?.iterator()?.forEach {
                            val res = recurseFind(it, guid)
                            if (res != null) {
                                return res
                            }
                        }
                        return null
                    }
                }
                recurseFind(tree, guid)
            },
        ).handleOpeningFolderBookmarks(tree, BrowsingMode.Normal)

        assertTrue(showTabTrayInvoked)
        assertNotNull(tabsTrayOpenedToPrivateTabs)
        assertFalse(tabsTrayOpenedToPrivateTabs!!)
        verifyOrder {
            addNewTabUseCase.invoke(item.url!!, private = false)
            addNewTabUseCase.invoke(item.url!!, private = false)
            addNewTabUseCase.invoke(childItem.url!!, private = false)
            homeActivity.browsingModeManager.mode = BrowsingMode.Normal
        }
    }

    @Test
    fun `WHEN handle opening folder bookmarks in private tabs THEN all bookmarks in folder is opened in private tabs`() {
        var showTabTrayInvoked = false
        var tabsTrayOpenedToPrivateTabs: Boolean? = null
        createController(
            showTabTray = { openToPrivateTabsPage ->
                tabsTrayOpenedToPrivateTabs = openToPrivateTabsPage
                showTabTrayInvoked = true
            },
            loadBookmarkNode = { guid: String, _: Boolean ->
                fun recurseFind(item: BookmarkNode, guid: String): BookmarkNode? {
                    if (item.guid == guid) {
                        return item
                    } else {
                        item.children?.iterator()?.forEach {
                            val res = recurseFind(it, guid)
                            if (res != null) {
                                return res
                            }
                        }
                        return null
                    }
                }
                recurseFind(tree, guid)
            },
        ).handleOpeningFolderBookmarks(tree, BrowsingMode.Private)

        assertTrue(showTabTrayInvoked)
        assertNotNull(tabsTrayOpenedToPrivateTabs)
        assertTrue(tabsTrayOpenedToPrivateTabs!!)
        verifyOrder {
            addNewTabUseCase.invoke(item.url!!, private = true)
            addNewTabUseCase.invoke(item.url!!, private = true)
            addNewTabUseCase.invoke(childItem.url!!, private = true)
            homeActivity.browsingModeManager.mode = BrowsingMode.Private
        }
    }

    @Test
    fun `WHEN handle opening folder bookmarks with more than max items THEN warning is invoked`() {
        var warningInvoked = false

        mockkConstructor(DefaultBookmarkController::class)
        createController(
            loadBookmarkNode = { _: String, _: Boolean ->
                largeTree
            },
            warnLargeOpenAll = { _: Int, _: () -> Unit -> warningInvoked = true },
        ).handleOpeningFolderBookmarks(tree, BrowsingMode.Normal)

        unmockkConstructor(DefaultBookmarkController::class)

        assertTrue(warningInvoked)
    }

    @Test
    fun `handleBookmarkDeletion for an item should properly call a passed in delegate`() {
        var deleteBookmarkNodesInvoked = false
        createController(
            deleteBookmarkNodes = { nodes, removeEvent ->
                assertEquals(setOf(item), nodes)
                assertEquals(BookmarkRemoveType.SINGLE, removeEvent)
                deleteBookmarkNodesInvoked = true
            },
        ).handleBookmarkDeletion(setOf(item), BookmarkRemoveType.SINGLE)

        assertTrue(deleteBookmarkNodesInvoked)
    }

    @Test
    fun `handleBookmarkDeletion for multiple bookmarks should properly call a passed in delegate`() {
        var deleteBookmarkNodesInvoked = false
        createController(
            deleteBookmarkNodes = { nodes, removeEvent ->
                assertEquals(setOf(item, subfolder), nodes)
                assertEquals(BookmarkRemoveType.MULTIPLE, removeEvent)
                deleteBookmarkNodesInvoked = true
            },
        ).handleBookmarkDeletion(setOf(item, subfolder), BookmarkRemoveType.MULTIPLE)

        assertTrue(deleteBookmarkNodesInvoked)
    }

    @Test
    fun `handleBookmarkDeletion for a folder should properly call the delete folder delegate`() {
        var deleteBookmarkFolderInvoked = false
        createController(
            deleteBookmarkFolder = { nodes ->
                assertEquals(setOf(subfolder), nodes)
                deleteBookmarkFolderInvoked = true
            },
        ).handleBookmarkFolderDeletion(setOf(subfolder))

        assertTrue(deleteBookmarkFolderInvoked)
    }

    @Test
    fun `handleRequestSync dispatches actions in the correct order`() = runTestOnMain {
        every { homeActivity.components.backgroundServices.accountManager } returns mockk(relaxed = true)
        coEvery { homeActivity.bookmarkStorage.getBookmark(any()) } returns tree

        createController().handleRequestSync()

        verifyOrder {
            bookmarkStore.dispatch(BookmarkFragmentAction.StartSync)
            bookmarkStore.dispatch(BookmarkFragmentAction.FinishSync)
        }
    }

    @Test
    fun `handleBackPressed with one item in backstack should trigger handleBackPressed in NavController`() = runTestOnMain {
        every { bookmarkStore.state.guidBackstack } returns listOf(tree.guid)
        every { bookmarkStore.state.tree } returns tree

        createController().handleBackPressed()

        verify {
            navController.popBackStack()
        }
    }

    private fun createController(
        loadBookmarkNode: suspend (String, Boolean) -> BookmarkNode? = { _, _ -> null },
        showSnackbar: (String) -> Unit = { _ -> },
        deleteBookmarkNodes: (Set<BookmarkNode>, BookmarkRemoveType) -> Unit = { _, _ -> },
        deleteBookmarkFolder: (Set<BookmarkNode>) -> Unit = { _ -> },
        showTabTray: (Boolean) -> Unit = { },
        warnLargeOpenAll: (Int, () -> Unit) -> Unit = { _: Int, _: () -> Unit -> },
    ): BookmarkController {
        return DefaultBookmarkController(
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
            showTabTray = showTabTray,
            warnLargeOpenAll = warnLargeOpenAll,
        )
    }
}

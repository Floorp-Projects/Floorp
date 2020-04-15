/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import androidx.core.content.getSystemService
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Services
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components

@SuppressWarnings("TooManyFunctions", "LargeClass")
class BookmarkControllerTest {

    private lateinit var controller: BookmarkController

    private val context: Context = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val showSnackbar: (String) -> Unit = mockk(relaxed = true)
    private val deleteBookmarkNodes: (Set<BookmarkNode>, Event) -> Unit = mockk(relaxed = true)
    private val deleteBookmarkFolder: (BookmarkNode) -> Unit = mockk(relaxed = true)
    private val invokePendingDeletion: () -> Unit = mockk(relaxed = true)

    private val homeActivity: HomeActivity = mockk(relaxed = true)
    private val services: Services = mockk(relaxed = true)

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
        // needed for mocking 'getSystemService<ClipboardManager>()'
        mockkStatic(
            "androidx.core.content.ContextCompat",
            "android.content.ClipData",
            "org.mozilla.fenix.ext.ContextKt"
        )

        every { homeActivity.components.services } returns services
        every { navController.currentDestination } returns NavDestination("").apply {
            id = R.id.bookmarkFragment
        }

        controller = DefaultBookmarkController(
            context = homeActivity,
            navController = navController,
            showSnackbar = showSnackbar,
            deleteBookmarkNodes = deleteBookmarkNodes,
            deleteBookmarkFolder = deleteBookmarkFolder,
            invokePendingDeletion = invokePendingDeletion
        )
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
    fun `handleBookmarkExpand should navigate to the 'Bookmark' fragment`() {
        controller.handleBookmarkExpand(tree)

        verify {
            invokePendingDeletion.invoke()
            navController.navigate(
                BookmarkFragmentDirections.actionBookmarkFragmentSelf(tree.guid),
                null
            )
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
    fun `handleBookmarkSelected should show a toast when selecting a folder`() {
        val errorMessage = context.getString(R.string.bookmark_cannot_edit_root)

        controller.handleBookmarkSelected(root)

        verify {
            showSnackbar(errorMessage)
        }
    }

    @Test
    fun `handleCopyUrl should copy bookmark url to clipboard and show a toast`() {
        val clipboardManager: ClipboardManager = mockk(relaxed = true)
        val urlCopiedMessage = context.getString(R.string.url_copied)
        every { any<Context>().getSystemService<ClipboardManager>() } returns clipboardManager
        every { ClipData.newPlainText(any(), any()) } returns mockk(relaxed = true)

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
        controller.handleBookmarkFolderDeletion(subfolder)

        verify {
            deleteBookmarkFolder(subfolder)
        }
    }

    @Test
    fun `handleBackPressed should trigger handleBackPressed in NavController`() {
        controller.handleBackPressed()

        verify {
            invokePendingDeletion.invoke()
            navController.popBackStack()
        }
    }
}

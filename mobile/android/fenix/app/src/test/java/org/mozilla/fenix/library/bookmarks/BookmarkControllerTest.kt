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
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BrowsingMode
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbarPresenter
import org.mozilla.fenix.components.Services
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components

@SuppressWarnings("TooManyFunctions", "LargeClass")
class BookmarkControllerTest {

    private lateinit var controller: BookmarkController

    private val context: Context = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val snackbarPresenter: FenixSnackbarPresenter = mockk(relaxed = true)
    private val deleteBookmarkNodes: (Set<BookmarkNode>, Event) -> Unit = mockk(relaxed = true)

    private val homeActivity: HomeActivity = mockk(relaxed = true)
    private val services: Services = mockk(relaxed = true)

    private val item = BookmarkNode(BookmarkNodeType.ITEM, "456", "123", 0, "Mozilla", "http://mozilla.org", null)
    private val subfolder = BookmarkNode(BookmarkNodeType.FOLDER, "987", "123", 0, "Subfolder", null, listOf())
    private val childItem = BookmarkNode(
        BookmarkNodeType.ITEM, "987", "123", 2, "Firefox", "https://www.mozilla.org/en-US/firefox/", null
    )
    private val tree = BookmarkNode(
        BookmarkNodeType.FOLDER, "123", null, 0, "Mobile", null, listOf(item, item, childItem, subfolder)
    )
    private val root = BookmarkNode(
        BookmarkNodeType.FOLDER, BookmarkRoot.Root.id, null, 0, BookmarkRoot.Root.name, null, null
    )

    @Before
    fun setup() {
        // needed for mocking 'getSystemService<ClipboardManager>()'
        mockkStatic(
            "androidx.core.content.ContextCompat",
            "android.content.ClipData"
        )

        every { homeActivity.components.services } returns services
        every { navController.currentDestination } returns NavDestination("").apply { id = R.id.bookmarkFragment }

        controller = DefaultBookmarkController(
            context = homeActivity,
            navController = navController,
            snackbarPresenter = snackbarPresenter,
            deleteBookmarkNodes = deleteBookmarkNodes
        )
    }

    @Test
    fun `handleBookmarkTapped should load the bookmark in a new tab`() {
        controller.handleBookmarkTapped(item)

        verifyOrder {
            homeActivity.browsingModeManager.mode = BrowsingMode.Normal
            homeActivity.openToBrowserAndLoad(item.url!!, true, BrowserDirection.FromBookmarks)
        }
    }

    @Test
    fun `handleBookmarkExpand should navigate to the 'Bookmark' fragment`() {
        controller.handleBookmarkExpand(tree)

        verify {
            navController.navigate(BookmarkFragmentDirections.actionBookmarkFragmentSelf(tree.guid))
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
            navController.navigate(BookmarkFragmentDirections.actionBookmarkFragmentToBookmarkEditFragment(item.guid))
        }
    }

    @Test
    fun `handleBookmarkSelected should show a toast when selecting a folder`() {
        val errorMessage = context.getString(R.string.bookmark_cannot_edit_root)

        controller.handleBookmarkSelected(root)

        verify {
            snackbarPresenter.present(errorMessage, any(), any(), any())
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
            snackbarPresenter.present(urlCopiedMessage, any(), any(), any())
        }
    }

    @Test
    fun `handleBookmarkSharing should navigate to the 'Share' fragment`() {
        controller.handleBookmarkSharing(item)

        verify {
            navController.navigate(
                BookmarkFragmentDirections.actionBookmarkFragmentToShareFragment(
                    item.url,
                    item.title
                )
            )
        }
    }

    @Test
    fun `handleOpeningBookmark should open the bookmark a new 'Normal' tab`() {
        controller.handleOpeningBookmark(item, BrowsingMode.Normal)

        verifyOrder {
            homeActivity.browsingModeManager.mode = BrowsingMode.Normal
            homeActivity.openToBrowserAndLoad(item.url!!, true, BrowserDirection.FromBookmarks)
        }
    }

    @Test
    fun `handleOpeningBookmark should open the bookmark a new 'Private' tab`() {
        controller.handleOpeningBookmark(item, BrowsingMode.Private)

        verifyOrder {
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
    fun `handleBookmarkDeletion for a folder should properly call a passed in delegate`() {
        controller.handleBookmarkDeletion(setOf(subfolder), Event.RemoveBookmarkFolder)

        verify {
            deleteBookmarkNodes(setOf(subfolder), Event.RemoveBookmarkFolder)
        }
    }

    @Test
    fun `handleBackPressed should trigger handleBackPressed in NavController`() {
        controller.handleBackPressed()

        verify {
            navController.popBackStack()
        }
    }

    @Test
    fun `handleSigningIn should trigger 'PairingSignIn`() {
        controller.handleSigningIn()

        verify {
            services.launchPairingSignIn(homeActivity, navController)
        }
    }
}

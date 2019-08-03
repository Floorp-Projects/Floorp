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
import io.mockk.called
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbarPresenter
import org.mozilla.fenix.components.Services
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components

class BookmarkFragmentInteractorTest {

    private lateinit var interactor: BookmarkFragmentInteractor

    private val context: Context = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val bookmarkStore = spyk(BookmarkStore(BookmarkState(null)))
    private val sharedViewModel: BookmarksSharedViewModel = mockk(relaxed = true)
    private val snackbarPresenter: FenixSnackbarPresenter = mockk(relaxed = true)
    private val deleteBookmarkNodes: (Set<BookmarkNode>, Event) -> Unit = mockk(relaxed = true)

    private val applicationContext: FenixApplication = mockk(relaxed = true)
    private val homeActivity: HomeActivity = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)

    private val item = BookmarkNode(BookmarkNodeType.ITEM, "456", "123", 0, "Mozilla", "http://mozilla.org", null)
    private val separator = BookmarkNode(BookmarkNodeType.SEPARATOR, "789", "123", 1, null, null, null)
    private val subfolder = BookmarkNode(BookmarkNodeType.FOLDER, "987", "123", 0, "Subfolder", null, listOf())
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
        BookmarkNodeType.FOLDER, "123", null, 0, "Mobile", null, listOf(item, separator, childItem, subfolder)
    )
    private val root = BookmarkNode(
        BookmarkNodeType.FOLDER, BookmarkRoot.Root.id, null, 0, BookmarkRoot.Root.name, null, null
    )

    @Before
    fun setup() {
        mockkStatic(
            "org.mozilla.fenix.ext.ContextKt",
            "androidx.core.content.ContextCompat",
            "android.content.ClipData"
        )
        every { any<Context>().asActivity() } returns homeActivity
        every { context.applicationContext } returns applicationContext
        every { applicationContext.components.analytics.metrics } returns metrics
        every { navController.currentDestination } returns NavDestination("").apply { id = R.id.bookmarkFragment }
        every { bookmarkStore.dispatch(any()) } returns mockk()

        interactor =
            BookmarkFragmentInteractor(
                context,
                navController,
                bookmarkStore,
                sharedViewModel,
                snackbarPresenter,
                deleteBookmarkNodes
            )
    }

    @Test
    fun `update bookmarks tree`() {
        interactor.change(tree)

        verify {
            bookmarkStore.dispatch(BookmarkAction.Change(tree))
        }
    }

    @Test
    fun `open a bookmark item`() {
        interactor.open(item)

        val url = item.url!!
        verifyOrder {
            homeActivity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = true,
                from = BrowserDirection.FromBookmarks
            )
        }
        metrics.track(Event.OpenedBookmark)
    }

    @Test
    fun `expand a level of bookmarks`() {
        interactor.open(tree)

        verify {
            navController.navigate(BookmarkFragmentDirections.actionBookmarkFragmentSelf(tree.guid))
        }
    }

    @Test
    fun `switch between bookmark selection modes`() {
        interactor.switchMode(BookmarkState.Mode.Normal)

        verify {
            homeActivity.invalidateOptionsMenu()
        }
    }

    @Test
    fun `press the edit bookmark button`() {
        interactor.edit(item)

        verify {
            navController.navigate(BookmarkFragmentDirections.actionBookmarkFragmentToBookmarkEditFragment(item.guid))
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
        interactor.deselectAll()

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
        val clipboardManager: ClipboardManager = mockk(relaxed = true)
        every { any<Context>().getSystemService<ClipboardManager>() } returns clipboardManager
        every { ClipData.newPlainText(any(), any()) } returns mockk(relaxed = true)

        interactor.copy(item)

        verify {
            metrics.track(Event.CopyBookmark)
        }
    }

    @Test
    fun `share a bookmark item`() {
        interactor.share(item)

        verifyOrder {
            navController.navigate(
                BookmarkFragmentDirections.actionBookmarkFragmentToShareFragment(
                    item.url,
                    item.title
                )
            )
            metrics.track(Event.ShareBookmark)
        }
    }

    @Test
    fun `open a bookmark item in a new tab`() {
        interactor.openInNewTab(item)

        val url = item.url!!
        verifyOrder {
            homeActivity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = true,
                from = BrowserDirection.FromBookmarks
            )
            metrics.track(Event.OpenedBookmarkInNewTab)
        }
    }

    @Test
    fun `open a bookmark item in a private tab`() {
        interactor.openInPrivateTab(item)

        val url = item.url!!
        verifyOrder {
            homeActivity.openToBrowserAndLoad(
                searchTermOrURL = url,
                newTab = true,
                from = BrowserDirection.FromBookmarks
            )
            metrics.track(Event.OpenedBookmarkInPrivateTab)
        }
    }

    @Test
    fun `delete a bookmark item`() {
        interactor.delete(setOf(item))

        verify {
            deleteBookmarkNodes(setOf(item), Event.RemoveBookmark)
        }
    }

    @Test
    fun `delete a bookmark folder`() {
        interactor.delete(setOf(subfolder))

        verify {
            deleteBookmarkNodes(setOf(subfolder), Event.RemoveBookmarkFolder)
        }
    }

    @Test
    fun `delete multiple bookmarks`() {
        interactor.delete(setOf(item, subfolder))

        verify {
            deleteBookmarkNodes(setOf(item, subfolder), Event.RemoveBookmarks)
        }
    }

    @Test
    fun `press the back button`() {
        interactor.backPressed()

        verify {
            navController.popBackStack()
        }
    }

    @Test
    fun `clicked sign in on bookmarks screen`() {
        val services: Services = mockk(relaxed = true)
        every { context.components.services } returns services

        interactor.clickedSignIn()

        verify {
            context.components.services
            services.launchPairingSignIn(context, navController)
        }
    }

    @Test
    fun `got signed in signal on bookmarks screen`() {
        interactor.signedIn()

        verify {
            sharedViewModel.signedIn.postValue(true)
        }
    }

    @Test
    fun `got signed out signal on bookmarks screen`() {
        interactor.signedOut()

        verify {
            sharedViewModel.signedIn.postValue(false)
        }
    }
}

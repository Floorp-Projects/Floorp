/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class BookmarkDeselectNavigationListenerTest {

    private val basicNode = BookmarkNode(
        BookmarkNodeType.ITEM,
        BookmarkRoot.Root.id,
        parentGuid = null,
        position = 0u,
        title = null,
        url = null,
        dateAdded = 0,
        children = null,
    )

    @Test
    fun `add listener on resume and remove on destroy`() {
        val navController: NavController = mockk(relaxed = true)
        val listener = BookmarkDeselectNavigationListener(navController, mockk(), mockk())

        listener.onResume(mockk())
        verify { navController.addOnDestinationChangedListener(listener) }

        listener.onDestroy(mockk())
        verify { navController.removeOnDestinationChangedListener(listener) }
    }

    @Test
    fun `deselect when navigating to a different fragment`() {
        val destination: NavDestination = mockk()
        every { destination.id } returns R.id.homeFragment

        val interactor: BookmarkViewInteractor = mockk(relaxed = true)
        val listener = BookmarkDeselectNavigationListener(mockk(), mockk(), interactor)

        listener.onDestinationChanged(mockk(), destination, mockk())
        verify { interactor.onAllBookmarksDeselected() }
    }

    @Test
    fun `deselect when navigating to a different folder`() {
        val arguments = BookmarkFragmentArgs(currentRoot = "mock-guid").toBundle()
        val destination: NavDestination = mockk()
        every { destination.id } returns R.id.bookmarkFragment

        val viewModel: BookmarksSharedViewModel = mockk()
        val interactor: BookmarkViewInteractor = mockk(relaxed = true)
        val listener = BookmarkDeselectNavigationListener(mockk(), viewModel, interactor)

        every { viewModel.selectedFolder } returns null
        listener.onDestinationChanged(mockk(), destination, arguments)
        verify { interactor.onAllBookmarksDeselected() }

        every { viewModel.selectedFolder } returns basicNode.copy(guid = "some-other-guid")
        listener.onDestinationChanged(mockk(), destination, arguments)
        verify { interactor.onAllBookmarksDeselected() }
    }

    @Test
    fun `do not deselect when navigating to the same folder`() {
        val arguments = BookmarkFragmentArgs(currentRoot = "mock-guid").toBundle()
        val destination: NavDestination = mockk()
        every { destination.id } returns R.id.bookmarkFragment

        val viewModel: BookmarksSharedViewModel = mockk()
        val interactor: BookmarkViewInteractor = mockk(relaxed = true)
        val listener = BookmarkDeselectNavigationListener(mockk(), viewModel, interactor)

        every { viewModel.selectedFolder } returns basicNode.copy(guid = "mock-guid")
        listener.onDestinationChanged(mockk(), destination, arguments)
        verify(exactly = 0) { interactor.onAllBookmarksDeselected() }
    }
}

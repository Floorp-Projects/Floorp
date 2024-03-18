/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.search.SearchEngine
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu

class SearchDialogInteractorTest {

    lateinit var searchController: SearchDialogController
    lateinit var interactor: SearchDialogInteractor

    @Before
    fun setup() {
        searchController = mockk(relaxed = true)
        interactor = SearchDialogInteractor(
            searchController,
        )
    }

    @Test
    fun onUrlCommitted() {
        interactor.onUrlCommitted("test")

        verify {
            searchController.handleUrlCommitted("test")
        }
    }

    @Test
    fun onEditingCanceled() = runTest {
        interactor.onEditingCanceled()

        verify {
            searchController.handleEditingCancelled()
        }
    }

    @Test
    fun onTextChanged() {
        val interactor = SearchDialogInteractor(searchController)

        interactor.onTextChanged("test")

        verify { searchController.handleTextChanged("test") }
    }

    @Test
    fun onUrlTapped() {
        interactor.onUrlTapped("test")

        verify {
            searchController.handleUrlTapped("test")
        }
    }

    @Test
    fun onSearchTermsTapped() {
        interactor.onSearchTermsTapped("test")
        verify {
            searchController.handleSearchTermsTapped("test")
        }
    }

    @Test
    fun `WHEN the search term from history is tapped THEN delegate the event to the controller`() {
        interactor.onHistorySearchTermTapped("test")
        verify {
            searchController.handleSearchTermsTapped("test")
        }
    }

    @Test
    fun onSearchShortcutEngineSelected() {
        val searchEngine: SearchEngine = mockk(relaxed = true)

        interactor.onSearchShortcutEngineSelected(searchEngine)

        verify { searchController.handleSearchShortcutEngineSelected(searchEngine) }
    }

    @Test
    fun onClickSearchEngineSettings() {
        interactor.onClickSearchEngineSettings()

        verify {
            searchController.handleClickSearchEngineSettings()
        }
    }

    @Test
    fun onExistingSessionSelected() {
        val sessionId = "mozilla"

        interactor.onExistingSessionSelected(sessionId)

        verify {
            searchController.handleExistingSessionSelected(sessionId)
        }
    }

    @Test
    fun onCameraPermissionsNeeded() {
        interactor.onCameraPermissionsNeeded()

        verify {
            searchController.handleCameraPermissionsNeeded()
        }
    }

    @Test
    fun onMenuItemTapped() {
        val item = SearchSelectorMenu.Item.SearchSettings

        interactor.onMenuItemTapped(item)

        verify {
            searchController.handleMenuItemTapped(item)
        }
    }
}

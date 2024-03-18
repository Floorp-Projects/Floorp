/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state

import androidx.navigation.NavController
import androidx.navigation.NavOptions
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.verify
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentDirections
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore
import org.mozilla.fenix.library.history.HistoryItemTimeGroup

class HistoryNavigationMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN regular history item clicked THEN item is opened in browser`() = runTest {
        var openedInBrowser = false
        val url = "url"
        val history = History.Regular(0, "title", url, 0, HistoryItemTimeGroup.timeGroupForTimestamp(0))
        val middleware = HistoryNavigationMiddleware(
            navController = mock(),
            openToBrowser = { item ->
                if (item.url == url) {
                    openedInBrowser = true
                }
            },
            onBackPressed = { },
            scope = this,
        )
        val store =
            HistoryFragmentStore(HistoryFragmentState.initial, middleware = listOf(middleware))

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(history)).joinBlocking()
        advanceUntilIdle()

        assertTrue(openedInBrowser)
    }

    @Test
    fun `GIVEN selected items WHEN the last selected item is clicked THEN last item is not opened`() = runTest {
        var openedInBrowser = false
        val url = "url"
        val history = History.Regular(0, "title", url, 0, HistoryItemTimeGroup.timeGroupForTimestamp(0))
        val middleware = HistoryNavigationMiddleware(
            navController = mock(),
            openToBrowser = { item ->
                if (item.url == url) {
                    openedInBrowser = true
                }
            },
            onBackPressed = { },
            scope = this,
        )
        val state = HistoryFragmentState.initial.copy(
            mode = HistoryFragmentState.Mode.Editing(selectedItems = setOf(history)),
        )
        val store =
            HistoryFragmentStore(initialState = state, middleware = listOf(middleware))

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(history)).joinBlocking()
        advanceUntilIdle()

        assertFalse(openedInBrowser)
    }

    @Test
    fun `WHEN group history item clicked THEN navigate to history metadata fragment`() = runTest {
        val title = "title"
        val history = History.Group(0, title, 0, HistoryItemTimeGroup.timeGroupForTimestamp(0), listOf())
        val navController = mock<NavController>()
        whenever(navController.navigate(directions = any(), navOptions = any())).thenAnswer { }
        val middleware = HistoryNavigationMiddleware(
            navController = navController,
            openToBrowser = { },
            onBackPressed = { },
            scope = this,
        )
        val store =
            HistoryFragmentStore(HistoryFragmentState.initial, middleware = listOf(middleware))

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(history)).joinBlocking()
        advanceUntilIdle()

        verify(navController).navigate(
            directions = any(),
            navOptions = any(),
        )
    }

    @Test
    fun `WHEN recently closed is requested to be entered THEN nav controller navigates to it`() = runTest {
        val navController = mock<NavController>()
        val middleware = HistoryNavigationMiddleware(
            navController = navController,
            openToBrowser = { },
            onBackPressed = { },
            scope = this,
        )
        val store =
            HistoryFragmentStore(HistoryFragmentState.initial, middleware = listOf(middleware))

        store.dispatch(HistoryFragmentAction.EnterRecentlyClosed).joinBlocking()
        advanceUntilIdle()

        verify(navController).navigate(
            HistoryFragmentDirections.actionGlobalRecentlyClosed(),
            NavOptions.Builder().setPopUpTo(R.id.recentlyClosedFragment, true).build(),
        )
    }

    @Test
    fun `GIVEN mode is editing WHEN back pressed THEN no navigation happens`() = runTest {
        var onBackPressed = false
        val middleware = HistoryNavigationMiddleware(
            navController = mock(),
            openToBrowser = { },
            onBackPressed = { onBackPressed = true },
            scope = this,
        )
        val store =
            HistoryFragmentStore(
                HistoryFragmentState.initial.copy(
                    mode = HistoryFragmentState.Mode.Editing(
                        setOf(),
                    ),
                ),
                middleware = listOf(middleware),
            )

        store.dispatch(HistoryFragmentAction.BackPressed).joinBlocking()
        advanceUntilIdle()

        assertFalse(onBackPressed)
    }

    @Test
    fun `GIVEN mode is not editing WHEN back pressed THEN onBackPressed callback invoked`() = runTest {
        var onBackPressed = false
        val middleware = HistoryNavigationMiddleware(
            navController = mock(),
            openToBrowser = { },
            onBackPressed = { onBackPressed = true },
            scope = this,
        )
        val store =
            HistoryFragmentStore(HistoryFragmentState.initial, middleware = listOf(middleware))

        store.dispatch(HistoryFragmentAction.BackPressed).joinBlocking()
        advanceUntilIdle()

        assertTrue(onBackPressed)
    }

    @Test
    fun `WHEN search is clicked THEN search navigated to`() = runTest {
        val navController = mock<NavController>()
        val middleware = HistoryNavigationMiddleware(
            navController = navController,
            openToBrowser = { },
            onBackPressed = { },
            scope = this,
        )

        val store =
            HistoryFragmentStore(HistoryFragmentState.initial, middleware = listOf(middleware))

        store.dispatch(HistoryFragmentAction.SearchClicked).joinBlocking()
        advanceUntilIdle()

        verify(navController).navigateSafe(R.id.historyFragment, HistoryFragmentDirections.actionGlobalSearchDialog(null))
    }
}

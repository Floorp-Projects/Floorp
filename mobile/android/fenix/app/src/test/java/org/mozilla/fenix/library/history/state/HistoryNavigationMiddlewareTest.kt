package org.mozilla.fenix.library.history.state

import androidx.navigation.NavController
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
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryFragmentAction
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
}

package org.mozilla.fenix.library.history.state

import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.HistoryMetadataAction
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.anyLong
import org.mockito.Mockito.verify
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.history.DefaultPagedHistoryProvider
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
import org.mozilla.fenix.library.history.RemoveTimeFrame
import org.mozilla.fenix.library.history.toPendingDeletionHistory

class HistoryStorageMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val appStore = mock<AppStore>()
    private lateinit var captureBrowserActions: CaptureActionsMiddleware<BrowserState, BrowserAction>
    private lateinit var browserStore: BrowserStore
    private val provider = mock<DefaultPagedHistoryProvider>()
    private lateinit var storage: PlacesHistoryStorage

    @Before
    fun setup() {
        storage = mock()
        captureBrowserActions = CaptureActionsMiddleware()
        browserStore = BrowserStore(middleware = listOf(captureBrowserActions))
    }

    @Test
    fun `WHEN items are deleted THEN action to add to pending deletion is dispatched and snackbar is shown`() = runTestOnMain {
        var snackbarCalled = false
        val history = setOf(History.Regular(0, "title", "url", 0, HistoryItemTimeGroup.timeGroupForTimestamp(0)))
        val middleware = HistoryStorageMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            historyProvider = provider,
            historyStorage = storage,
            undoDeleteSnackbar = { _, _, _ -> snackbarCalled = true },
            onTimeFrameDeleted = { },
            scope = this,
        )
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.DeleteItems(history)).joinBlocking()
        store.waitUntilIdle()

        assertTrue(snackbarCalled)
        verify(appStore).dispatch(AppAction.AddPendingDeletionSet(history.toPendingDeletionHistory()))
    }

    @Test
    fun `WHEN items are deleted THEN undo dispatches action to remove them from pending deletion state`() = runTestOnMain {
        var snackbarCalled = false
        val history = setOf(History.Regular(0, "title", "url", 0, HistoryItemTimeGroup.timeGroupForTimestamp(0)))
        val middleware = HistoryStorageMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            historyProvider = provider,
            historyStorage = storage,
            undoDeleteSnackbar = { _, undo, _ ->
                runBlocking { undo(history) }
                snackbarCalled = true
            },
            onTimeFrameDeleted = { },
            scope = this,
        )
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.DeleteItems(history)).joinBlocking()
        store.waitUntilIdle()

        assertTrue(snackbarCalled)
        verify(appStore).dispatch(AppAction.AddPendingDeletionSet(history.toPendingDeletionHistory()))
        verify(appStore).dispatch(AppAction.UndoPendingDeletionSet(history.toPendingDeletionHistory()))
    }

    @Test
    fun `WHEN regular items are deleted and not undone THEN items are removed from storage`() = runTestOnMain {
        var snackbarCalled = false
        val history = setOf(History.Regular(0, "title", "url", 0, HistoryItemTimeGroup.timeGroupForTimestamp(0)))
        val middleware = HistoryStorageMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            historyProvider = provider,
            historyStorage = storage,
            undoDeleteSnackbar = { _, _, delete ->
                runBlocking { delete(history) }
                snackbarCalled = true
            },
            onTimeFrameDeleted = { },
            scope = this,
        )
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.DeleteItems(history)).joinBlocking()
        store.waitUntilIdle()

        assertTrue(snackbarCalled)
        verify(storage).deleteVisitsFor(history.first().url)
    }

    @Test
    fun `WHEN group items are deleted and not undone THEN items are removed from provider and browser store updated`() = runTestOnMain {
        var snackbarCalled = false
        val history = setOf(History.Group(0, "title", 0, HistoryItemTimeGroup.timeGroupForTimestamp(0), listOf()))
        val middleware = HistoryStorageMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            historyProvider = provider,
            historyStorage = storage,
            undoDeleteSnackbar = { _, _, delete ->
                runBlocking { delete(history) }
                snackbarCalled = true
            },
            onTimeFrameDeleted = { },
            scope = this,
        )
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.DeleteItems(history)).joinBlocking()
        store.waitUntilIdle()
        browserStore.waitUntilIdle()

        assertTrue(snackbarCalled)
        captureBrowserActions.assertFirstAction(HistoryMetadataAction.DisbandSearchGroupAction::class)
        verify(provider).deleteMetadataSearchGroup(history.first())
    }

    @Test
    fun `WHEN a null time frame is deleted THEN browser store is informed, storage deletes everything, and callback is invoked`() = runTestOnMain {
        var callbackInvoked = false
        val middleware = HistoryStorageMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            historyProvider = provider,
            historyStorage = storage,
            undoDeleteSnackbar = mock(),
            onTimeFrameDeleted = { callbackInvoked = true },
            scope = this,
        )
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.DeleteTimeRange(null)).joinBlocking()
        store.waitUntilIdle()
        browserStore.waitUntilIdle()

        assertTrue(callbackInvoked)
        assertEquals(HistoryFragmentState.Mode.Normal, store.state.mode)
        captureBrowserActions.assertFirstAction(RecentlyClosedAction.RemoveAllClosedTabAction::class)
        captureBrowserActions.assertLastAction(EngineAction.PurgeHistoryAction::class) {}
        verify(storage).deleteEverything()
    }

    @Test
    fun `WHEN a specified time frame is deleted THEN browser store is informed, storage deletes time frame, and callback is invoked`() = runTestOnMain {
        var callbackInvoked = false
        val middleware = HistoryStorageMiddleware(
            appStore = appStore,
            browserStore = browserStore,
            historyProvider = provider,
            historyStorage = storage,
            undoDeleteSnackbar = mock(),
            onTimeFrameDeleted = { callbackInvoked = true },
            scope = this,
        )
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.DeleteTimeRange(RemoveTimeFrame.LastHour)).joinBlocking()
        store.waitUntilIdle()
        browserStore.waitUntilIdle()
        this.advanceUntilIdle()

        assertTrue(callbackInvoked)
        assertFalse(store.state.isDeletingItems)
        captureBrowserActions.assertFirstAction(RecentlyClosedAction.RemoveAllClosedTabAction::class)
        captureBrowserActions.assertLastAction(EngineAction.PurgeHistoryAction::class) {}
        verify(storage).deleteVisitsBetween(anyLong(), anyLong())
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import androidx.navigation.NavController
import io.mockk.coVerifyOrder
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.PlacesHistoryStorage
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.history.DefaultPagedHistoryProvider
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class HistoryControllerTest {
    private val historyItem = History.Regular(
        0,
        "title",
        "url",
        0.toLong(),
        HistoryItemTimeGroup.timeGroupForTimestamp(0),
    )

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private val store: HistoryFragmentStore = mockk(relaxed = true)
    private val appStore: AppStore = mockk(relaxed = true)
    private val browserStore: BrowserStore = mockk(relaxed = true)
    private val historyStorage: PlacesHistoryStorage = mockk(relaxed = true)
    private val state: HistoryFragmentState = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val historyProvider: DefaultPagedHistoryProvider = mockk(relaxed = true)

    @Before
    fun setUp() {
        every { store.state } returns state
    }

    @Test
    fun onPressHistoryItemInNormalMode() {
        var actualHistoryItem: History? = null
        val controller = createController(
            openInBrowser = {
                actualHistoryItem = it
            },
        )
        controller.handleOpen(historyItem)
        assertEquals(historyItem, actualHistoryItem)
    }

    @Test
    fun onPressHistoryItemInEditMode() {
        every { state.mode } returns HistoryFragmentState.Mode.Editing(setOf())

        createController().handleSelect(historyItem)

        verify {
            store.dispatch(HistoryFragmentAction.AddItemForRemoval(historyItem))
        }
    }

    @Test
    fun onPressSelectedHistoryItemInEditMode() {
        every { state.mode } returns HistoryFragmentState.Mode.Editing(setOf(historyItem))

        createController().handleDeselect(historyItem)

        verify {
            store.dispatch(HistoryFragmentAction.RemoveItemForRemoval(historyItem))
        }
    }

    @Test
    fun onSelectHistoryItemDuringSync() {
        every { state.mode } returns HistoryFragmentState.Mode.Syncing

        createController().handleSelect(historyItem)

        verify(exactly = 0) {
            store.dispatch(HistoryFragmentAction.AddItemForRemoval(historyItem))
        }
    }

    @Test
    fun onBackPressedInNormalMode() {
        every { state.mode } returns HistoryFragmentState.Mode.Normal

        assertFalse(createController().handleBackPressed())
    }

    @Test
    fun onBackPressedInEditMode() {
        every { state.mode } returns HistoryFragmentState.Mode.Editing(setOf())

        assertTrue(createController().handleBackPressed())
        verify {
            store.dispatch(HistoryFragmentAction.ExitEditMode)
        }
    }

    @Test
    fun onModeSwitched() {
        var invalidateOptionsMenuInvoked = false
        val controller = createController(
            invalidateOptionsMenu = {
                invalidateOptionsMenuInvoked = true
            },
        )

        controller.handleModeSwitched()
        assertTrue(invalidateOptionsMenuInvoked)
    }

    @Test
    fun onSearch() {
        val controller = createController()

        controller.handleSearch()
        verify {
            navController.navigateSafe(
                R.id.historyFragment,
                HistoryFragmentDirections.actionGlobalSearchDialog(sessionId = null),
            )
        }
    }

    @Test
    fun onDeleteTimeRange() {
        var displayDeleteTimeRangeInvoked = false
        val controller = createController(
            displayDeleteTimeRange = {
                displayDeleteTimeRangeInvoked = true
            },
        )

        controller.handleDeleteTimeRange()
        assertTrue(displayDeleteTimeRangeInvoked)
    }

    @Test
    fun `WHEN user confirms history deletion GIVEN timeFrame is null THEN delete all history, log the event, purge history and remove recently closed items`() {
        val controller = createController()
        assertNull(org.mozilla.fenix.GleanMetrics.History.removedAll.testGetValue())

        controller.handleDeleteTimeRangeConfirmed(null)
        coVerifyOrder {
            store.dispatch(HistoryFragmentAction.EnterDeletionMode)
            historyStorage.deleteEverything()
            browserStore.dispatch(RecentlyClosedAction.RemoveAllClosedTabAction)
            browserStore.dispatch(EngineAction.PurgeHistoryAction)
            store.dispatch(HistoryFragmentAction.ExitDeletionMode)
        }

        assertNotNull(org.mozilla.fenix.GleanMetrics.History.removedAll.testGetValue())
    }

    @Test
    fun `WHEN user confirms history deletion GIVEN timeFrame is lastHour THEN delete visits between the time frame, log the event, purge history and remove recently closed items`() {
        val controller = createController()
        assertNull(org.mozilla.fenix.GleanMetrics.History.removedLastHour.testGetValue())

        controller.handleDeleteTimeRangeConfirmed(RemoveTimeFrame.LastHour)
        coVerifyOrder {
            store.dispatch(HistoryFragmentAction.EnterDeletionMode)
            historyStorage.deleteVisitsBetween(any(), any())
            browserStore.dispatch(RecentlyClosedAction.RemoveAllClosedTabAction)
            browserStore.dispatch(EngineAction.PurgeHistoryAction)
            store.dispatch(HistoryFragmentAction.ExitDeletionMode)
        }

        assertNotNull(org.mozilla.fenix.GleanMetrics.History.removedLastHour.testGetValue())
    }

    @Test
    fun `WHEN user confirms history deletion GIVEN timeFrame is todayAndYesterday THEN delete visits between the time frame, log the event, purge history and remove recently closed items`() {
        val controller = createController()
        assertNull(org.mozilla.fenix.GleanMetrics.History.removedTodayAndYesterday.testGetValue())

        controller.handleDeleteTimeRangeConfirmed(RemoveTimeFrame.TodayAndYesterday)
        coVerifyOrder {
            store.dispatch(HistoryFragmentAction.EnterDeletionMode)
            historyStorage.deleteVisitsBetween(any(), any())
            browserStore.dispatch(RecentlyClosedAction.RemoveAllClosedTabAction)
            browserStore.dispatch(EngineAction.PurgeHistoryAction)
            store.dispatch(HistoryFragmentAction.ExitDeletionMode)
        }

        assertNotNull(org.mozilla.fenix.GleanMetrics.History.removedTodayAndYesterday.testGetValue())
    }

    @Test
    fun onDeleteSome() {
        val itemsToDelete = setOf(historyItem)
        var actualItems: Set<History>? = null
        val controller = createController(
            deleteHistoryItems = { items ->
                actualItems = items
            },
        )

        controller.handleDeleteSome(itemsToDelete)
        assertEquals(itemsToDelete, actualItems)
    }

    @Test
    fun onRequestSync() {
        var syncHistoryInvoked = false
        createController(
            syncHistory = {
                syncHistoryInvoked = true
            },
        ).handleRequestSync()

        coVerifyOrder {
            store.dispatch(HistoryFragmentAction.StartSync)
            store.dispatch(HistoryFragmentAction.FinishSync)
        }

        assertTrue(syncHistoryInvoked)
    }

    @Suppress("LongParameterList")
    private fun createController(
        openInBrowser: (History) -> Unit = { _ -> },
        displayDeleteTimeRange: () -> Unit = {},
        onTimeFrameDeleted: () -> Unit = {},
        invalidateOptionsMenu: () -> Unit = {},
        deleteHistoryItems: (Set<History>) -> Unit = { _ -> },
        syncHistory: suspend () -> Unit = {},
    ): HistoryController {
        return DefaultHistoryController(
            store,
            appStore,
            browserStore,
            historyStorage,
            historyProvider,
            navController,
            scope,
            openInBrowser,
            displayDeleteTimeRange,
            onTimeFrameDeleted,
            invalidateOptionsMenu,
            { items, _, _ -> deleteHistoryItems.invoke(items) },
            syncHistory,
        )
    }
}

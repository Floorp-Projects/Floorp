package org.mozilla.fenix.library.history.state

import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.library.history.History
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore
import org.mozilla.fenix.library.history.HistoryItemTimeGroup
import org.robolectric.RobolectricTestRunner
import org.mozilla.fenix.GleanMetrics.History as GleanHistory

@RunWith(RobolectricTestRunner::class)
class HistoryTelemetryMiddlewareTest {
    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val middleware = HistoryTelemetryMiddleware(isInPrivateMode = false)

    @Test
    fun `GIVEN no items selected WHEN regular history item clicked THEN telemetry recorded`() {
        val history = History.Regular(0, "title", "url", 0, HistoryItemTimeGroup.timeGroupForTimestamp(0))
        val store = HistoryFragmentStore(
            initialState = HistoryFragmentState.initial,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(history)).joinBlocking()

        assertNotNull(GleanHistory.openedItem.testGetValue())
    }

    @Test
    fun `GIVEN items selected WHEN regular history item clicked THEN no telemetry recorded`() {
        val history = History.Regular(0, "title", "url", 0, HistoryItemTimeGroup.timeGroupForTimestamp(0))
        val state = HistoryFragmentState.initial.copy(
            mode = HistoryFragmentState.Mode.Editing(selectedItems = setOf(history)),
        )
        val store = HistoryFragmentStore(
            initialState = state,
            middleware = listOf(middleware),
        )

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(history)).joinBlocking()

        assertNull(GleanHistory.openedItem.testGetValue())
    }

    @Test
    fun `WHEN group history item clicked THEN record telemetry`() {
        val history = History.Group(0, "title", 0, HistoryItemTimeGroup.timeGroupForTimestamp(0), listOf())
        val store =
            HistoryFragmentStore(HistoryFragmentState.initial, middleware = listOf(middleware))

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(history)).joinBlocking()

        assertNotNull(GleanHistory.searchTermGroupTapped.testGetValue())
    }
}

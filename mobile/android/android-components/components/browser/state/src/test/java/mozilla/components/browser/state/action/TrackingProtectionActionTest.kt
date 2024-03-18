/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class TrackingProtectionActionTest {
    private lateinit var tab: TabSessionState
    private lateinit var store: BrowserStore

    @Before
    fun setUp() {
        tab = createTab("https://www.mozilla.org")

        store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )
    }

    private fun tabState(): TabSessionState = store.state.findTab(tab.id)!!
    private fun trackingProtectionState() = tabState().trackingProtection

    @Test
    fun `ToggleAction - Updates enabled flag of TrackingProtectionState`() {
        assertFalse(trackingProtectionState().enabled)

        store.dispatch(TrackingProtectionAction.ToggleAction(tabId = tab.id, enabled = true))
            .joinBlocking()

        assertTrue(trackingProtectionState().enabled)

        store.dispatch(TrackingProtectionAction.ToggleAction(tabId = tab.id, enabled = true))
            .joinBlocking()

        assertTrue(trackingProtectionState().enabled)

        store.dispatch(TrackingProtectionAction.ToggleAction(tabId = tab.id, enabled = false))
            .joinBlocking()

        assertFalse(trackingProtectionState().enabled)

        store.dispatch(TrackingProtectionAction.ToggleAction(tabId = tab.id, enabled = true))
            .joinBlocking()

        assertTrue(trackingProtectionState().enabled)
    }

    @Test
    fun `ToggleExclusionListAction - Updates enabled flag of TrackingProtectionState`() {
        assertFalse(trackingProtectionState().ignoredOnTrackingProtection)

        store.dispatch(
            TrackingProtectionAction.ToggleExclusionListAction(
                tabId = tab.id,
                excluded = true,
            ),
        ).joinBlocking()

        assertTrue(trackingProtectionState().ignoredOnTrackingProtection)

        store.dispatch(
            TrackingProtectionAction.ToggleExclusionListAction(
                tabId = tab.id,
                excluded = true,
            ),
        ).joinBlocking()

        assertTrue(trackingProtectionState().ignoredOnTrackingProtection)

        store.dispatch(
            TrackingProtectionAction.ToggleExclusionListAction(
                tabId = tab.id,
                excluded = false,
            ),
        ).joinBlocking()

        assertFalse(trackingProtectionState().ignoredOnTrackingProtection)

        store.dispatch(
            TrackingProtectionAction.ToggleExclusionListAction(
                tabId = tab.id,
                excluded = true,
            ),
        ).joinBlocking()

        assertTrue(trackingProtectionState().ignoredOnTrackingProtection)
    }

    @Test
    fun `TrackerBlockedAction - Adds tackers to TrackingProtectionState`() {
        assertTrue(trackingProtectionState().blockedTrackers.isEmpty())
        assertTrue(trackingProtectionState().loadedTrackers.isEmpty())

        store.dispatch(TrackingProtectionAction.TrackerBlockedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        assertEquals(1, trackingProtectionState().blockedTrackers.size)
        assertEquals(0, trackingProtectionState().loadedTrackers.size)

        store.dispatch(TrackingProtectionAction.TrackerBlockedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        store.dispatch(TrackingProtectionAction.TrackerBlockedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        assertEquals(3, trackingProtectionState().blockedTrackers.size)
        assertEquals(0, trackingProtectionState().loadedTrackers.size)
    }

    @Test
    fun `TrackerLoadedAction - Adds tackers to TrackingProtectionState`() {
        assertTrue(trackingProtectionState().blockedTrackers.isEmpty())
        assertTrue(trackingProtectionState().loadedTrackers.isEmpty())

        store.dispatch(TrackingProtectionAction.TrackerLoadedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        assertEquals(0, trackingProtectionState().blockedTrackers.size)
        assertEquals(1, trackingProtectionState().loadedTrackers.size)

        store.dispatch(TrackingProtectionAction.TrackerLoadedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        store.dispatch(TrackingProtectionAction.TrackerLoadedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        assertEquals(0, trackingProtectionState().blockedTrackers.size)
        assertEquals(3, trackingProtectionState().loadedTrackers.size)
    }

    @Test
    fun `ClearTrackers - Removes trackers from TrackingProtectionState`() {
        store.dispatch(TrackingProtectionAction.TrackerBlockedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        store.dispatch(TrackingProtectionAction.TrackerBlockedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        store.dispatch(TrackingProtectionAction.TrackerLoadedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        store.dispatch(TrackingProtectionAction.TrackerLoadedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        store.dispatch(TrackingProtectionAction.TrackerLoadedAction(tabId = tab.id, tracker = mock()))
            .joinBlocking()

        assertEquals(2, trackingProtectionState().blockedTrackers.size)
        assertEquals(3, trackingProtectionState().loadedTrackers.size)

        store.dispatch(TrackingProtectionAction.ClearTrackersAction(tab.id)).joinBlocking()

        assertEquals(0, trackingProtectionState().blockedTrackers.size)
        assertEquals(0, trackingProtectionState().loadedTrackers.size)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import io.mockk.mockk
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.content.blocking.TrackerLog
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class TrackingProtectionStoreTest {

    val tab: SessionState = mockk(relaxed = true)

    @Test
    fun enterDetailsMode() = runBlocking {
        val initialState = defaultState()
        val store = TrackingProtectionStore(initialState)

        store.dispatch(
            TrackingProtectionAction.EnterDetailsMode(
                TrackingProtectionCategory.FINGERPRINTERS,
                true
            )
        )
            .join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            TrackingProtectionState.Mode.Details(TrackingProtectionCategory.FINGERPRINTERS, true)
        )
        assertEquals(store.state.lastAccessedCategory, TrackingProtectionCategory.FINGERPRINTERS.name)
    }

    @Test
    fun exitDetailsMode() = runBlocking {
        val initialState = detailsState()
        val store = TrackingProtectionStore(initialState)

        store.dispatch(TrackingProtectionAction.ExitDetailsMode).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            TrackingProtectionState.Mode.Normal
        )
        assertEquals(store.state.lastAccessedCategory, initialState.lastAccessedCategory)
    }

    @Test
    fun trackerListChanged() = runBlocking {
        val initialState = defaultState()
        val store = TrackingProtectionStore(initialState)
        val tracker = TrackerLog("url", listOf())

        store.dispatch(TrackingProtectionAction.TrackerLogChange(listOf(tracker))).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            listOf(tracker),
            store.state.listTrackers
        )
    }

    @Test
    fun urlChanged() = runBlocking {
        val initialState = defaultState()
        val store = TrackingProtectionStore(initialState)

        store.dispatch(TrackingProtectionAction.UrlChange("newURL")).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            "newURL",
            store.state.url
        )
    }

    @Test
    fun onChange() = runBlocking {
        val initialState = defaultState()
        val store = TrackingProtectionStore(initialState)
        val tracker = TrackerLog("url", listOf(), listOf(), cookiesHasBeenBlocked = false)

        store.dispatch(
            TrackingProtectionAction.Change(
                "newURL",
                false,
                listOf(tracker),
                TrackingProtectionState.Mode.Details(
                    TrackingProtectionCategory.FINGERPRINTERS,
                    true
                )
            )
        ).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            "newURL",
            store.state.url
        )
        assertEquals(
            false,
            store.state.isTrackingProtectionEnabled
        )
        assertEquals(
            listOf(tracker),
            store.state.listTrackers
        )
        assertEquals(
            TrackingProtectionState.Mode.Details(TrackingProtectionCategory.FINGERPRINTERS, true),
            store.state.mode
        )
    }

    private fun defaultState(): TrackingProtectionState = TrackingProtectionState(
        tab = tab,
        url = "www.mozilla.org",
        isTrackingProtectionEnabled = true,
        listTrackers = listOf(),
        mode = TrackingProtectionState.Mode.Normal,
        lastAccessedCategory = ""
    )

    private fun detailsState(): TrackingProtectionState = TrackingProtectionState(
        tab = tab,
        url = "www.mozilla.org",
        isTrackingProtectionEnabled = true,
        listTrackers = listOf(),
        mode = TrackingProtectionState.Mode.Details(TrackingProtectionCategory.CRYPTOMINERS, true),
        lastAccessedCategory = TrackingProtectionCategory.CRYPTOMINERS.name
    )
}

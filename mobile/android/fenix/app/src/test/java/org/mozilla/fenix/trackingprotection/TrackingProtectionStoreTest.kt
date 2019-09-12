/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import io.mockk.mockk
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.content.blocking.Tracker
import org.junit.Test

class TrackingProtectionStoreTest {

    val session: Session = mockk(relaxed = true)

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
    }

    @Test
    fun trackerBlockingChanged() = runBlocking {
        val initialState = defaultState()
        val store = TrackingProtectionStore(initialState)

        store.dispatch(TrackingProtectionAction.TrackerBlockingChanged(false)).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            TrackingProtectionState.Mode.Normal
        )
        assertEquals(
            false,
            store.state.isTrackingProtectionEnabled
        )
    }

    @Test
    fun trackerListChanged() = runBlocking {
        val initialState = defaultState()
        val store = TrackingProtectionStore(initialState)
        val tracker = Tracker("url", listOf())

        store.dispatch(TrackingProtectionAction.TrackerListChange(listOf(tracker))).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            listOf(tracker),
            store.state.listTrackers
        )
    }

    @Test
    fun trackerLoadedListChanged() = runBlocking {
        val initialState = defaultState()
        val store = TrackingProtectionStore(initialState)
        val tracker = Tracker("url", listOf())

        store.dispatch(TrackingProtectionAction.TrackerLoadedListChange(listOf(tracker))).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            listOf(tracker),
            store.state.listTrackersLoaded
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
        val tracker = Tracker("url", listOf())

        store.dispatch(
            TrackingProtectionAction.Change(
                "newURL",
                false,
                listOf(tracker),
                listOf(),
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
            listOf<Tracker>(),
            store.state.listTrackersLoaded
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
        session = session,
        url = "www.mozilla.org",
        isTrackingProtectionEnabled = true,
        listTrackers = listOf(),
        listTrackersLoaded = listOf(),
        mode = TrackingProtectionState.Mode.Normal
    )

    private fun detailsState(): TrackingProtectionState = TrackingProtectionState(
        session = session,
        url = "www.mozilla.org",
        isTrackingProtectionEnabled = true,
        listTrackers = listOf(),
        listTrackersLoaded = listOf(),
        mode = TrackingProtectionState.Mode.Details(TrackingProtectionCategory.CRYPTOMINERS, true)
    )
}

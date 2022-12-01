/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.content.blocking.TrackerLog
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertTrue
import org.junit.Test

class ProtectionsStoreTest {

    val tab: SessionState = mockk(relaxed = true)

    @Test
    fun enterDetailsMode() = runTest {
        val initialState = defaultState()
        val store = ProtectionsStore(initialState)

        store.dispatch(
            ProtectionsAction.EnterDetailsMode(
                TrackingProtectionCategory.FINGERPRINTERS,
                true,
            ),
        )
            .join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            ProtectionsState.Mode.Details(TrackingProtectionCategory.FINGERPRINTERS, true),
        )
        assertEquals(store.state.lastAccessedCategory, TrackingProtectionCategory.FINGERPRINTERS.name)
    }

    @Test
    fun exitDetailsMode() = runTest {
        val initialState = detailsState()
        val store = ProtectionsStore(initialState)

        store.dispatch(ProtectionsAction.ExitDetailsMode).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            ProtectionsState.Mode.Normal,
        )
        assertEquals(store.state.lastAccessedCategory, initialState.lastAccessedCategory)
    }

    @Test
    fun trackerListChanged() = runTest {
        val initialState = defaultState()
        val store = ProtectionsStore(initialState)
        val tracker = TrackerLog("url", listOf())

        store.dispatch(ProtectionsAction.TrackerLogChange(listOf(tracker))).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            listOf(tracker),
            store.state.listTrackers,
        )
    }

    @Test
    fun urlChanged() = runTest {
        val initialState = defaultState()
        val store = ProtectionsStore(initialState)

        store.dispatch(ProtectionsAction.UrlChange("newURL")).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            "newURL",
            store.state.url,
        )
    }

    @Test
    fun onChange() = runTest {
        val initialState = defaultState()
        val store = ProtectionsStore(initialState)
        val tracker = TrackerLog("url", listOf(), listOf(), cookiesHasBeenBlocked = false)

        store.dispatch(
            ProtectionsAction.Change(
                "newURL",
                isTrackingProtectionEnabled = false,
                isCookieBannerHandlingEnabled = false,
                listTrackers = listOf(tracker),
                mode = ProtectionsState.Mode.Details(
                    TrackingProtectionCategory.FINGERPRINTERS,
                    true,
                ),
            ),
        ).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            "newURL",
            store.state.url,
        )
        assertEquals(
            false,
            store.state.isTrackingProtectionEnabled,
        )
        assertEquals(
            false,
            store.state.isCookieBannerHandlingEnabled,
        )
        assertEquals(
            listOf(tracker),
            store.state.listTrackers,
        )
        assertEquals(
            ProtectionsState.Mode.Details(TrackingProtectionCategory.FINGERPRINTERS, true),
            store.state.mode,
        )
    }

    @Test
    fun `ProtectionsAction - ToggleCookieBannerHandlingProtectionEnabled`() = runTest {
        val initialState = defaultState()
        val store = ProtectionsStore(initialState)

        store.dispatch(
            ProtectionsAction.ToggleCookieBannerHandlingProtectionEnabled(
                isEnabled = true,
            ),
        ).join()

        assertTrue(store.state.isCookieBannerHandlingEnabled)
    }

    private fun defaultState(): ProtectionsState = ProtectionsState(
        tab = tab,
        url = "www.mozilla.org",
        isTrackingProtectionEnabled = true,
        isCookieBannerHandlingEnabled = false,
        listTrackers = listOf(),
        mode = ProtectionsState.Mode.Normal,
        lastAccessedCategory = "",
    )

    private fun detailsState(): ProtectionsState = ProtectionsState(
        tab = tab,
        url = "www.mozilla.org",
        isTrackingProtectionEnabled = true,
        isCookieBannerHandlingEnabled = false,
        listTrackers = listOf(),
        mode = ProtectionsState.Mode.Details(TrackingProtectionCategory.CRYPTOMINERS, true),
        lastAccessedCategory = TrackingProtectionCategory.CRYPTOMINERS.name,
    )
}

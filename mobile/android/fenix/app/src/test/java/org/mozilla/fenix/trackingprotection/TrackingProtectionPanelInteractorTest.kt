/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import io.mockk.mockk
import io.mockk.verify
import org.junit.Test

import org.junit.Assert.assertEquals

class TrackingProtectionPanelInteractorTest {

    @Test
    fun openDetails() {
        val store: TrackingProtectionStore = mockk(relaxed = true)
        val interactor = TrackingProtectionPanelInteractor(store, {}, {})
        interactor.openDetails(TrackingProtectionCategory.FINGERPRINTERS, true)
        verify {
            store.dispatch(
                TrackingProtectionAction.EnterDetailsMode(
                    TrackingProtectionCategory.FINGERPRINTERS,
                    true
                )
            )
        }
    }

    @Test
    fun openDetailsForRedirectTrackers() {
        val store: TrackingProtectionStore = mockk(relaxed = true)
        val interactor =
            TrackingProtectionPanelInteractor(store, {}, {})
        interactor.openDetails(TrackingProtectionCategory.REDIRECT_TRACKERS, true)
        verify {
            store.dispatch(
                TrackingProtectionAction.EnterDetailsMode(
                    TrackingProtectionCategory.REDIRECT_TRACKERS,
                    true
                )
            )
        }
    }

    @Test
    fun selectTrackingProtectionSettings() {
        var openSettings = false
        val interactor = TrackingProtectionPanelInteractor(
            mockk(),
            { },
            { openSettings = true }
        )
        interactor.selectTrackingProtectionSettings()
        assertEquals(true, openSettings)
    }

    @Test
    fun onBackPressed() {
        val store: TrackingProtectionStore = mockk(relaxed = true)
        val interactor =
            TrackingProtectionPanelInteractor(store, {}, {})
        interactor.onBackPressed()
        verify { store.dispatch(TrackingProtectionAction.ExitDetailsMode) }
    }
}

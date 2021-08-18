/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.widget.FrameLayout
import androidx.core.view.isVisible
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.spyk
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.QuicksettingsTrackingProtectionBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.trackingprotection.TrackingProtectionState
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionViewTest {

    private lateinit var view: TrackingProtectionView
    private lateinit var binding: QuicksettingsTrackingProtectionBinding
    private lateinit var interactor: TrackingProtectionInteractor

    @MockK(relaxed = true)
    private lateinit var settings: Settings

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        interactor = mockk(relaxed = true)
        view = spyk(TrackingProtectionView(FrameLayout(testContext), interactor, settings))
        binding = view.binding
    }

    @Test
    fun `WHEN updating THEN bind checkbox`() {
        val websiteUrl = "https://mozilla.org"
        val state = TrackingProtectionState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            listTrackers = listOf(),
            mode = TrackingProtectionState.Mode.Normal,
            lastAccessedCategory = ""
        )

        every { settings.shouldUseTrackingProtection } returns true

        view.update(state)

        assertTrue(binding.root.isVisible)
        assertTrue(binding.trackingProtectionSwitch.switchWidget.isChecked)
    }

    @Test
    fun `GIVEN TP is globally off WHEN updating THEN hide the TP section`() {
        val websiteUrl = "https://mozilla.org"
        val state = TrackingProtectionState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            listTrackers = listOf(),
            mode = TrackingProtectionState.Mode.Normal,
            lastAccessedCategory = ""
        )

        every { settings.shouldUseTrackingProtection } returns false

        view.update(state)

        assertFalse(binding.root.isVisible)
    }

    @Test
    fun `WHEN updateDetailsSection is called THEN update the visibility of the section`() {
        every { settings.shouldUseTrackingProtection } returns false

        view.updateDetailsSection(false)

        assertFalse(binding.trackingProtectionDetails.isVisible)

        view.updateDetailsSection(true)

        assertTrue(binding.trackingProtectionDetails.isVisible)
    }
}

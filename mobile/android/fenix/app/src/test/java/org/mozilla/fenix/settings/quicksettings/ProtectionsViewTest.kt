/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.View
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
import org.mozilla.fenix.databinding.QuicksettingsProtectionsPanelBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.quicksettings.protections.ProtectionsInteractor
import org.mozilla.fenix.settings.quicksettings.protections.ProtectionsView
import org.mozilla.fenix.trackingprotection.CookieBannerUIMode
import org.mozilla.fenix.trackingprotection.ProtectionsState
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class ProtectionsViewTest {

    private lateinit var view: ProtectionsView
    private lateinit var binding: QuicksettingsProtectionsPanelBinding
    private lateinit var interactor: ProtectionsInteractor
    private var trackingProtectionDivider: View = spyk(View(testContext))

    @MockK(relaxed = true)
    private lateinit var settings: Settings

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        interactor = mockk(relaxed = true)
        view = spyk(
            ProtectionsView(
                FrameLayout(testContext),
                trackingProtectionDivider,
                interactor,
                settings,
            ),
        )
        binding = view.binding
    }

    @Test
    fun `WHEN updating THEN bind checkbox`() {
        val websiteUrl = "https://mozilla.org"
        val state = ProtectionsState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            cookieBannerUIMode = CookieBannerUIMode.ENABLE,
            listTrackers = listOf(),
            mode = ProtectionsState.Mode.Normal,
            lastAccessedCategory = "",
        )

        every { settings.shouldUseTrackingProtection } returns true

        view.update(state)

        assertTrue(binding.root.isVisible)
        assertTrue(binding.trackingProtectionSwitch.isChecked)
    }

    @Test
    fun `GIVEN TP is globally off WHEN updating THEN hide the TP section`() {
        val websiteUrl = "https://mozilla.org"
        val state = ProtectionsState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            cookieBannerUIMode = CookieBannerUIMode.ENABLE,
            listTrackers = listOf(),
            mode = ProtectionsState.Mode.Normal,
            lastAccessedCategory = "",
        )

        every { settings.shouldUseTrackingProtection } returns false

        view.update(state)

        assertFalse(binding.trackingProtectionSwitch.isVisible)
    }

    @Test
    fun `GIVEN cookie banners handling is globally off WHEN updating THEN hide the cookie banner section`() {
        val websiteUrl = "https://mozilla.org"
        val state = ProtectionsState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            cookieBannerUIMode = CookieBannerUIMode.ENABLE,
            listTrackers = listOf(),
            mode = ProtectionsState.Mode.Normal,
            lastAccessedCategory = "",
        )

        every { settings.shouldShowCookieBannerUI } returns true
        every { settings.shouldUseCookieBannerPrivateMode } returns false

        view.update(state)

        assertFalse(binding.cookieBannerItem.isVisible)
    }

    @Test
    fun `GIVEN cookie banners handling UI feature flag is off WHEN updating THEN hide the cookie banner section`() {
        val websiteUrl = "https://mozilla.org"
        val state = ProtectionsState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            cookieBannerUIMode = CookieBannerUIMode.ENABLE,
            listTrackers = listOf(),
            mode = ProtectionsState.Mode.Normal,
            lastAccessedCategory = "",
        )

        every { settings.shouldShowCookieBannerUI } returns false
        every { settings.shouldUseCookieBannerPrivateMode } returns false

        view.update(state)

        assertFalse(binding.cookieBannerItem.isVisible)
    }

    @Test
    fun `GIVEN cookie banners handling mode is hide WHEN updating THEN hide the cookie banner section`() {
        val websiteUrl = "https://mozilla.org"
        val state = ProtectionsState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = true,
            cookieBannerUIMode = CookieBannerUIMode.HIDE,
            listTrackers = listOf(),
            mode = ProtectionsState.Mode.Normal,
            lastAccessedCategory = "",
        )

        every { settings.shouldShowCookieBannerUI } returns true
        every { settings.shouldUseCookieBannerPrivateMode } returns true

        view.update(state)

        assertFalse(binding.cookieBannerItem.isVisible)
    }

    @Test
    fun `WHEN updateDetailsSection is called THEN update the visibility of the section`() {
        every { settings.shouldUseTrackingProtection } returns false

        view.updateDetailsSection(false)

        assertFalse(binding.trackingProtectionDetails.isVisible)

        view.updateDetailsSection(true)

        assertTrue(binding.trackingProtectionDetails.isVisible)
    }

    @Test
    fun `WHEN all the views from protectionView are gone THEN tracking protection divider is gone`() {
        val websiteUrl = "https://mozilla.org"
        val state = ProtectionsState(
            tab = createTab(url = websiteUrl),
            url = websiteUrl,
            isTrackingProtectionEnabled = false,
            cookieBannerUIMode = CookieBannerUIMode.HIDE,
            listTrackers = listOf(),
            mode = ProtectionsState.Mode.Normal,
            lastAccessedCategory = "",
        )

        every { settings.shouldShowCookieBannerUI } returns true
        every { settings.shouldUseCookieBannerPrivateMode } returns true

        view.update(state)

        assertFalse(trackingProtectionDivider.isVisible)
    }
}

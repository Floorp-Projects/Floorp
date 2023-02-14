/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import androidx.fragment.app.FragmentActivity
import io.mockk.every
import io.mockk.mockk
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings
import org.robolectric.Robolectric

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionFragmentTest {

    @Test
    fun `UI component should match settings defaults`() {
        val settings = Settings(testContext)
        every { testContext.components.analytics } returns mockk(relaxed = true)
        every { testContext.components.settings } returns settings
        val settingsFragment = TrackingProtectionFragment()
        val activity = Robolectric.buildActivity(FragmentActivity::class.java).create().get()

        activity.supportFragmentManager.beginTransaction()
            .add(settingsFragment, "settingsFragment")
            .commitNow()

        val customCookiesCheckBox = settingsFragment.customCookies.isChecked
        val customCookiesCheckBoxSettings = settings.blockCookiesInCustomTrackingProtection

        val customCookiesSelect = settingsFragment.customCookiesSelect.value
        val customCookiesSelectSettings = settings.blockCookiesSelectionInCustomTrackingProtection

        val customTrackingContentCheckBox = settingsFragment.customTracking.isChecked
        val customTrackingContentCheckBoxSettings = settings.blockTrackingContentInCustomTrackingProtection

        val customTrackingContentSelect = settingsFragment.customTrackingSelect.value
        val customTrackingContentSelectSettings = settings.blockTrackingContentSelectionInCustomTrackingProtection

        val customCryptominersCheckBox = settingsFragment.customCryptominers.isChecked
        val customCryptominersCheckBoxSettings = settings.blockCryptominersInCustomTrackingProtection

        val customFingerprintersCheckBox = settingsFragment.customFingerprinters.isChecked
        val customFingerprintersCheckBoxSettings = settings.blockFingerprintersInCustomTrackingProtection

        val customRedirectTrackersCheckBox = settingsFragment.customRedirectTrackers.isChecked
        val customRedirectTrackersCheckBoxSettings = settings.blockRedirectTrackersInCustomTrackingProtection

        assertEquals(customCookiesCheckBoxSettings, customCookiesCheckBox)
        assertEquals(customCookiesSelectSettings, customCookiesSelect)
        assertEquals(customTrackingContentCheckBoxSettings, customTrackingContentCheckBox)
        assertEquals(customTrackingContentSelect, customTrackingContentSelectSettings)
        assertEquals(customCryptominersCheckBoxSettings, customCryptominersCheckBox)
        assertEquals(customFingerprintersCheckBoxSettings, customFingerprintersCheckBox)
        assertEquals(customRedirectTrackersCheckBoxSettings, customRedirectTrackersCheckBox)
    }
}

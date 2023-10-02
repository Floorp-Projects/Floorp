/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import com.google.android.material.switchmaterial.SwitchMaterial
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.concept.engine.webextension.EnableSource
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import org.junit.Before
import org.junit.Test

class InstalledAddonDetailsFragmentTest {

    private lateinit var fragment: InstalledAddonDetailsFragment
    private val addonManager = mockk<AddonManager>()

    @Before
    fun setup() {
        fragment = spyk(InstalledAddonDetailsFragment())
    }

    @Test
    fun `GIVEN add-on is supported and not disabled WHEN enabling it THEN the add-on is requested by the user`() {
        val addon = mockk<Addon>()
        every { addon.isDisabledAsUnsupported() } returns false
        every { addon.isSupported() } returns true
        every { fragment.addon } returns addon
        every { addonManager.enableAddon(any(), any(), any(), any()) } just Runs

        fragment.enableAddon(addonManager, {}, {})

        verify { addonManager.enableAddon(addon, EnableSource.USER, any(), any()) }
    }

    @Test
    fun `GIVEN add-on is supported and disabled as previously unsupported WHEN enabling it THEN the add-on is requested by both the app and the user`() {
        val addon = mockk<Addon>()
        every { addon.isDisabledAsUnsupported() } returns true
        every { addon.isSupported() } returns true
        every { fragment.addon } returns addon
        val capturedAddon = slot<Addon>()
        val capturedOnSuccess = slot<(Addon) -> Unit>()
        every {
            addonManager.enableAddon(
                capture(capturedAddon),
                any(),
                capture(capturedOnSuccess),
                any(),
            )
        } answers { capturedOnSuccess.captured.invoke(capturedAddon.captured) }

        fragment.enableAddon(addonManager, {}, {})

        verify { addonManager.enableAddon(addon, EnableSource.APP_SUPPORT, any(), any()) }
        verify { addonManager.enableAddon(capturedAddon.captured, EnableSource.USER, any(), any()) }
    }

    @Test
    fun `GIVEN blocklisted addon WHEN biding the enable switch THEN disable the switch`() {
        val addon = mockk<Addon>()
        val enableSwitch = mockk<SwitchMaterial>(relaxed = true)
        val privateBrowsingSwitch = mockk<SwitchMaterial>(relaxed = true)

        every { fragment.provideEnableSwitch() } returns enableSwitch
        every { fragment.providePrivateBrowsingSwitch() } returns privateBrowsingSwitch
        every { addon.isEnabled() } returns true
        every { addon.isDisabledAsBlocklisted() } returns true
        every { fragment.addon } returns addon

        fragment.bindEnableSwitch()

        verify { enableSwitch.isEnabled = false }
    }

    @Test
    fun `GIVEN enabled addon WHEN biding the enable switch THEN do not disable the switch`() {
        val addon = mockk<Addon>()
        val enableSwitch = mockk<SwitchMaterial>(relaxed = true)
        val privateBrowsingSwitch = mockk<SwitchMaterial>(relaxed = true)

        every { fragment.provideEnableSwitch() } returns enableSwitch
        every { fragment.providePrivateBrowsingSwitch() } returns privateBrowsingSwitch
        every { addon.isDisabledAsBlocklisted() } returns false
        every { addon.isDisabledAsNotCorrectlySigned() } returns false
        every { addon.isDisabledAsIncompatible() } returns false
        every { addon.isEnabled() } returns true
        every { fragment.addon } returns addon

        fragment.bindEnableSwitch()

        verify(exactly = 0) { enableSwitch.isEnabled = false }
    }

    @Test
    fun `GIVEN addon not correctly signed WHEN biding the enable switch THEN disable the switch`() {
        val addon = mockk<Addon>()
        val enableSwitch = mockk<SwitchMaterial>(relaxed = true)
        val privateBrowsingSwitch = mockk<SwitchMaterial>(relaxed = true)

        every { fragment.provideEnableSwitch() } returns enableSwitch
        every { fragment.providePrivateBrowsingSwitch() } returns privateBrowsingSwitch
        every { addon.isEnabled() } returns true
        every { addon.isDisabledAsBlocklisted() } returns false
        every { addon.isDisabledAsNotCorrectlySigned() } returns true
        every { fragment.addon } returns addon

        fragment.bindEnableSwitch()

        verify { enableSwitch.isEnabled = false }
    }

    @Test
    fun `GIVEN incompatible addon WHEN biding the enable switch THEN disable the switch`() {
        val addon = mockk<Addon>()
        val enableSwitch = mockk<SwitchMaterial>(relaxed = true)
        val privateBrowsingSwitch = mockk<SwitchMaterial>(relaxed = true)

        every { fragment.provideEnableSwitch() } returns enableSwitch
        every { fragment.providePrivateBrowsingSwitch() } returns privateBrowsingSwitch
        every { addon.isEnabled() } returns true
        every { addon.isDisabledAsBlocklisted() } returns false
        every { addon.isDisabledAsNotCorrectlySigned() } returns false
        every { addon.isDisabledAsIncompatible() } returns true
        every { fragment.addon } returns addon

        fragment.bindEnableSwitch()

        verify { enableSwitch.isEnabled = false }
    }
}

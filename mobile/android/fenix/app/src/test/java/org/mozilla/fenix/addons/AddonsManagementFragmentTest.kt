/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Context
import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.feature.addons.Addon
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R

class AddonsManagementFragmentTest {

    private lateinit var context: Context
    private lateinit var view: CoordinatorLayout
    private lateinit var fragment: AddonsManagementFragment

    private val addonNotSupportedErrorMessage = "not supported"
    private val addonAlreadyInstalledErrorMessage = "already installed"

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        view = mockk(relaxed = true)
        fragment = spyk(AddonsManagementFragment())
        every { fragment.context } returns context
        every { fragment.view } returns view
        every { fragment.showErrorSnackBar(any()) } returns Unit
        every { fragment.getString(R.string.addon_not_supported_error) } returns addonNotSupportedErrorMessage
        every { fragment.getString(R.string.addon_already_installed) } returns addonAlreadyInstalledErrorMessage
    }

    @Test
    fun `GIVEN add-on is installed from external source WHEN add-on is not supported THEN error is shown`() {
        val supportedAddons = listOf(
            Addon("1", downloadId = "d1"),
            Addon("2", downloadId = "d2"),
        )
        val installAddonId = "d3"
        fragment.installExternalAddon(supportedAddons, installAddonId)
        verify { fragment.showErrorSnackBar(addonNotSupportedErrorMessage) }
    }

    @Test
    fun `GIVEN add-on is installed from external source WHEN add-on is already installed THEN error is shown`() {
        val addon1 = Addon("1", downloadId = "d1", installedState = mockk())
        val addon2 = Addon("2", downloadId = "d2")
        val supportedAddons = listOf(addon1, addon2)

        fragment.installExternalAddon(supportedAddons, "d1")
        verify { fragment.showErrorSnackBar(addonAlreadyInstalledErrorMessage) }
    }
}

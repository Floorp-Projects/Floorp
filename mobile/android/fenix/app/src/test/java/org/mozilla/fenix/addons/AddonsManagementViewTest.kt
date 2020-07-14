/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import androidx.navigation.NavController
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.AddonsManagerAdapterDelegate
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.addons.AddonsManagementFragmentDirections.Companion.actionAddonsManagementFragmentToInstalledAddonDetails
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class AddonsManagementViewTest {

    @RelaxedMockK private lateinit var navController: NavController
    @RelaxedMockK private lateinit var showPermissionDialog: (Addon) -> Unit
    private lateinit var managementView: AddonsManagerAdapterDelegate

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        managementView = AddonsManagementView(navController, showPermissionDialog)
    }

    @Test
    fun `onAddonItemClicked shows installed details if addon is installed`() {
        val addon = mockk<Addon> {
            every { isInstalled() } returns true
        }
        managementView.onAddonItemClicked(addon)

        verify {
            navController.navigate(
                directionsEq(actionAddonsManagementFragmentToInstalledAddonDetails(addon))
            )
        }
    }

    @Test
    fun `onAddonItemClicked shows details if addon is not installed`() {
        val addon = mockk<Addon> {
            every { isInstalled() } returns false
        }
        managementView.onAddonItemClicked(addon)

        val expected = AddonsManagementFragmentDirections.actionAddonsManagementFragmentToAddonDetailsFragment(addon)
        verify {
            navController.navigate(directionsEq(expected))
        }
    }

    @Test
    fun `onInstallAddonButtonClicked shows permission dialog`() {
        val addon = mockk<Addon>()
        managementView.onInstallAddonButtonClicked(addon)
        verify { showPermissionDialog(addon) }
    }

    @Test
    fun `onNotYetSupportedSectionClicked shows not yet supported fragment`() {
        val addons = listOf<Addon>(mockk(), mockk())
        managementView.onNotYetSupportedSectionClicked(addons)

        val expected = AddonsManagementFragmentDirections.actionAddonsManagementFragmentToNotYetSupportedAddonFragment(
            addons.toTypedArray()
        )
        verify {
            navController.navigate(directionsEq(expected))
        }
    }
}

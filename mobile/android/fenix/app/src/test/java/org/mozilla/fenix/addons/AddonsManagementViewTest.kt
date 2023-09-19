/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.AddonsManagerAdapterDelegate
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.AddonsManagementFragmentDirections.Companion.actionAddonsManagementFragmentToAddonDetailsFragment
import org.mozilla.fenix.addons.AddonsManagementFragmentDirections.Companion.actionAddonsManagementFragmentToInstalledAddonDetails
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class AddonsManagementViewTest {

    @RelaxedMockK private lateinit var navController: NavController
    private lateinit var managementView: AddonsManagerAdapterDelegate
    private var showPermissionDialog: (Addon) -> Unit = { permissionDialogDisplayed = true }
    private var permissionDialogDisplayed = false

    private var onMoreAddonsButtonClicked: () -> Unit = { moreAddonsButtonClicked = true }
    private var moreAddonsButtonClicked = false

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        managementView = AddonsManagementView(navController, showPermissionDialog, onMoreAddonsButtonClicked)
    }

    @Test
    fun `onAddonItemClicked shows installed details if addon is installed`() {
        val addon = mockk<Addon> {
            every { isInstalled() } returns true
        }

        every { navController.currentDestination } returns NavDestination("").apply {
            id = R.id.addonsManagementFragment
        }

        managementView.onAddonItemClicked(addon)

        val expected = actionAddonsManagementFragmentToInstalledAddonDetails(addon)
        verify {
            navController.navigate(directionsEq(expected))
        }
    }

    @Test
    fun `onAddonItemClicked shows details if addon is not installed`() {
        val addon = mockk<Addon> {
            every { isInstalled() } returns false
        }

        every { navController.currentDestination } returns NavDestination("").apply {
            id = R.id.addonsManagementFragment
        }

        managementView.onAddonItemClicked(addon)

        val expected = actionAddonsManagementFragmentToAddonDetailsFragment(addon)
        verify {
            navController.navigate(directionsEq(expected))
        }
    }

    @Test
    fun `onAddonItemClicked on not installed addon does not navigate if not currently on addonsManagementFragment`() {
        val addon = mockk<Addon> {
            every { isInstalled() } returns false
        }

        every { navController.currentDestination } returns NavDestination("").apply {
            id = R.id.settingsFragment
        }

        managementView.onAddonItemClicked(addon)

        val expected = actionAddonsManagementFragmentToAddonDetailsFragment(addon)
        verify(exactly = 0) {
            navController.navigate(directionsEq(expected))
        }
    }

    @Test
    fun `onAddonItemClicked on installed addon does not navigate if not currently on addonsManagementFragment`() {
        val addon = mockk<Addon> {
            every { isInstalled() } returns true
        }

        every { navController.currentDestination } returns NavDestination("").apply {
            id = R.id.settingsFragment
        }

        managementView.onAddonItemClicked(addon)

        val expected = actionAddonsManagementFragmentToAddonDetailsFragment(addon)
        verify(exactly = 0) {
            navController.navigate(directionsEq(expected))
        }
    }

    @Test
    fun `onInstallAddonButtonClicked shows permission dialog`() {
        val addon = mockk<Addon>()
        managementView.onInstallAddonButtonClicked(addon)
        assertTrue(permissionDialogDisplayed)
    }

    @Test
    fun `onNotYetSupportedSectionClicked shows not yet supported fragment`() {
        val addons = listOf<Addon>(mockk(), mockk())
        managementView.onNotYetSupportedSectionClicked(addons)

        val expected = AddonsManagementFragmentDirections.actionAddonsManagementFragmentToNotYetSupportedAddonFragment(
            addons.toTypedArray(),
        )
        verify {
            navController.navigate(directionsEq(expected))
        }
    }

    @Test
    fun `onFindMoreAddonsButtonClicked calls onMoreAddonsButtonClicked`() {
        managementView.onFindMoreAddonsButtonClicked()
        assertTrue(moreAddonsButtonClicked)
    }
}

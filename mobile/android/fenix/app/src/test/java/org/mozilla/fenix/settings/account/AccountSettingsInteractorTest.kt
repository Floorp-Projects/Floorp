/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.R

class AccountSettingsInteractorTest {

    @Test
    fun onSyncNow() {
        var ranSyncNow = false

        val interactor = AccountSettingsInteractor(
            mockk(),
            { ranSyncNow = true },
            mockk(),
            mockk()
        )

        interactor.onSyncNow()

        assertEquals(ranSyncNow, true)
    }

    @Test
    fun onChangeDeviceName() {
        val store: AccountSettingsFragmentStore = mockk(relaxed = true)
        var invalidResponseInvoked = false
        val invalidNameResponse = { invalidResponseInvoked = true }

        val interactor = AccountSettingsInteractor(
            mockk(),
            mockk(),
            { true },
            store
        )

        assertTrue(interactor.onChangeDeviceName("New Name", invalidNameResponse))

        verify { store.dispatch(AccountSettingsFragmentAction.UpdateDeviceName("New Name")) }
        assertFalse(invalidResponseInvoked)
    }

    @Test
    fun onChangeDeviceNameSyncFalse() {
        val store: AccountSettingsFragmentStore = mockk(relaxed = true)
        var invalidResponseInvoked = false
        val invalidNameResponse = { invalidResponseInvoked = true }

        val interactor = AccountSettingsInteractor(
            mockk(),
            mockk(),
            { false },
            store
        )

        assertFalse(interactor.onChangeDeviceName("New Name", invalidNameResponse))

        verify { store wasNot Called }
        assertTrue(invalidResponseInvoked)
    }

    @Test
    fun onSignOut() {
        val navController: NavController = mockk(relaxed = true)
        every { navController.currentDestination } returns NavDestination("").apply { id = R.id.accountSettingsFragment }

        val interactor = AccountSettingsInteractor(
            navController,
            mockk(),
            mockk(),
            mockk()
        )

        interactor.onSignOut()

        verify {
            navController.navigate(
                AccountSettingsFragmentDirections.actionAccountSettingsFragmentToSignOutFragment(),
                null
            )
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.search.SearchState
import org.mozilla.fenix.settings.account.*

class AccountSettingsStoreTest {

    @Test
    fun onSyncFailed() {
        val initialState = AccountSettingsState()
        val store = AccountSettingsStore(initialState)

        .onSyncNow()

        assertEquals(ranSyncNow, true)
    }

    @Test
    fun onSyncEnded() {
        val store: AccountSettingsStore = mockk(relaxed = true)

        val interactor = AccountSettingsInteractor(
            mockk(),
            mockk(),
            { true },
            store
        )

        interactor.onChangeDeviceName("New Name") {}

        verify { store.dispatch(AccountSettingsAction.UpdateDeviceName("New Name")) }
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
            navController.navigate(AccountSettingsFragmentDirections.actionAccountSettingsFragmentToSignOutFragment())
        }
    }
}

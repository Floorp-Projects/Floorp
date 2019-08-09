/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.util.Log
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.search.SearchState
import org.mozilla.fenix.settings.account.*

class AccountSettingsStoreTest {

    @Test
    fun syncFailed() = runBlocking {
        val initialState = AccountSettingsState()
        val store = AccountSettingsStore(initialState)
        val duration = 1L

        store.dispatch(AccountSettingsAction.SyncFailed(duration)).join()
        assertNotSame(initialState, store.state)
        assertEquals(LastSyncTime.Failed(duration), store.state.lastSyncedDate)
    }

    @Test
    fun syncEnded() = runBlocking {
        val initialState = AccountSettingsState()
        val store = AccountSettingsStore(initialState)
        val duration = 1L

        store.dispatch(AccountSettingsAction.SyncEnded(duration)).join()
        assertNotSame(initialState, store.state)
        assertEquals(LastSyncTime.Success(duration), store.state.lastSyncedDate)
    }

    @Test
    fun signOut() = runBlocking {
        val initialState = AccountSettingsState()
        val store = AccountSettingsStore(initialState)
        val deviceName = "testing"

        store.dispatch(AccountSettingsAction.UpdateDeviceName(deviceName)).join()
        assertNotSame(initialState, store.state)
        assertEquals(deviceName, store.state.deviceName)
    }
}

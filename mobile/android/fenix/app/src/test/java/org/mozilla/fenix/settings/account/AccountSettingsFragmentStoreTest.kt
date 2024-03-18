/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class AccountSettingsFragmentStoreTest {

    @Test
    fun syncFailed() = runTest {
        val initialState = AccountSettingsFragmentState()
        val store = AccountSettingsFragmentStore(initialState)
        val duration = 1L

        store.dispatch(AccountSettingsFragmentAction.SyncFailed(duration)).join()
        assertNotSame(initialState, store.state)
        assertEquals(LastSyncTime.Failed(duration), store.state.lastSyncedDate)
    }

    @Test
    fun syncEnded() = runTest {
        val initialState = AccountSettingsFragmentState()
        val store = AccountSettingsFragmentStore(initialState)
        val duration = 1L

        store.dispatch(AccountSettingsFragmentAction.SyncEnded(duration)).join()
        assertNotSame(initialState, store.state)
        assertEquals(LastSyncTime.Success(duration), store.state.lastSyncedDate)
    }

    @Test
    fun signOut() = runTest {
        val initialState = AccountSettingsFragmentState()
        val store = AccountSettingsFragmentStore(initialState)
        val deviceName = "testing"

        store.dispatch(AccountSettingsFragmentAction.UpdateDeviceName(deviceName)).join()
        assertNotSame(initialState, store.state)
        assertEquals(deviceName, store.state.deviceName)
    }
}

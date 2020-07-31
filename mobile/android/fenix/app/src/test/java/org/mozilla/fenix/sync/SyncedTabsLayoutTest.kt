/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import androidx.navigation.NavController
import io.mockk.mockk
import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.R

class SyncedTabsLayoutTest {

    @Test
    fun `pull to refresh state`() {
        assertTrue(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.MULTIPLE_DEVICES_UNAVAILABLE))
        assertTrue(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.SYNC_ENGINE_UNAVAILABLE))
        assertTrue(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.NO_TABS_AVAILABLE))
        assertFalse(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.SYNC_NEEDS_REAUTHENTICATION))
        assertFalse(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.SYNC_UNAVAILABLE))
    }

    @Test
    fun `string resource for error`() {
        assertEquals(
            R.string.synced_tabs_connect_another_device,
            SyncedTabsLayout.stringResourceForError(ErrorType.MULTIPLE_DEVICES_UNAVAILABLE)
        )
        assertEquals(
            R.string.synced_tabs_enable_tab_syncing,
            SyncedTabsLayout.stringResourceForError(ErrorType.SYNC_ENGINE_UNAVAILABLE)
        )
        assertEquals(
            R.string.synced_tabs_sign_in_message,
            SyncedTabsLayout.stringResourceForError(ErrorType.SYNC_UNAVAILABLE)
        )
        assertEquals(
            R.string.synced_tabs_reauth,
            SyncedTabsLayout.stringResourceForError(ErrorType.SYNC_NEEDS_REAUTHENTICATION)
        )
        assertEquals(
            R.string.synced_tabs_no_tabs,
            SyncedTabsLayout.stringResourceForError(ErrorType.NO_TABS_AVAILABLE)
        )
    }

    @Test
    fun `get error item`() {
        val navController = mockk<NavController>()

        var errorItem = SyncedTabsLayout.getErrorItem(
            navController,
            ErrorType.MULTIPLE_DEVICES_UNAVAILABLE,
            R.string.synced_tabs_connect_another_device
        )
        assertNull((errorItem as SyncedTabsAdapter.AdapterItem.Error).navController)
        assertEquals(R.string.synced_tabs_connect_another_device, errorItem.descriptionResId)

        errorItem = SyncedTabsLayout.getErrorItem(
            navController,
            ErrorType.SYNC_ENGINE_UNAVAILABLE,
            R.string.synced_tabs_enable_tab_syncing
        )
        assertNull((errorItem as SyncedTabsAdapter.AdapterItem.Error).navController)
        assertEquals(R.string.synced_tabs_enable_tab_syncing, errorItem.descriptionResId)

        errorItem = SyncedTabsLayout.getErrorItem(
            navController,
            ErrorType.SYNC_NEEDS_REAUTHENTICATION,
            R.string.synced_tabs_reauth
        )
        assertNull((errorItem as SyncedTabsAdapter.AdapterItem.Error).navController)
        assertEquals(R.string.synced_tabs_reauth, errorItem.descriptionResId)

        errorItem = SyncedTabsLayout.getErrorItem(
            navController,
            ErrorType.NO_TABS_AVAILABLE,
            R.string.synced_tabs_no_tabs
        )
        assertNull((errorItem as SyncedTabsAdapter.AdapterItem.Error).navController)
        assertEquals(R.string.synced_tabs_no_tabs, errorItem.descriptionResId)

        errorItem = SyncedTabsLayout.getErrorItem(
            navController,
            ErrorType.SYNC_UNAVAILABLE,
            R.string.synced_tabs_sign_in_message
        )
        assertNotNull((errorItem as SyncedTabsAdapter.AdapterItem.Error).navController)
        assertEquals(R.string.synced_tabs_sign_in_message, errorItem.descriptionResId)
    }
}

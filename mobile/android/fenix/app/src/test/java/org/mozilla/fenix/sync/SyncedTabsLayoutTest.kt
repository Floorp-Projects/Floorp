/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
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
            R.string.synced_tabs_connect_to_sync_account,
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
}

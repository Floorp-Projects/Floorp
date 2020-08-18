/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SyncedTabsLayoutTest {

    @Test
    fun `pull to refresh state`() {
        assertTrue(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.MULTIPLE_DEVICES_UNAVAILABLE))
        assertTrue(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.SYNC_ENGINE_UNAVAILABLE))
        assertTrue(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.NO_TABS_AVAILABLE))
        assertFalse(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.SYNC_NEEDS_REAUTHENTICATION))
        assertFalse(SyncedTabsLayout.pullToRefreshEnableState(ErrorType.SYNC_UNAVAILABLE))
    }
}

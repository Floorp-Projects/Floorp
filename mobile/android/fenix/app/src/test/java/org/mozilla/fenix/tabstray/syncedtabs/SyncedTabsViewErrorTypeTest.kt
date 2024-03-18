/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.syncedtabs

import androidx.navigation.NavController
import io.mockk.mockk
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.ext.toSyncedTabsListItem

@RunWith(FenixRobolectricTestRunner::class)
class SyncedTabsViewErrorTypeTest {

    @Test
    fun `GIVEN synced tabs error types WHEN the synced tabs update process errors THEN the correct error text should be displayed`() {
        val context = testContext
        val navController: NavController = mockk(relaxed = true)
        val multipleDevicesUnavailable = SyncedTabsView.ErrorType.MULTIPLE_DEVICES_UNAVAILABLE.toSyncedTabsListItem(context, navController)
        val syncEngineUnavailable = SyncedTabsView.ErrorType.SYNC_ENGINE_UNAVAILABLE.toSyncedTabsListItem(context, navController)
        val syncNeedsReauthentication = SyncedTabsView.ErrorType.SYNC_NEEDS_REAUTHENTICATION.toSyncedTabsListItem(context, navController)
        val noTabsAvailable = SyncedTabsView.ErrorType.NO_TABS_AVAILABLE.toSyncedTabsListItem(context, navController)
        val syncUnavailable = SyncedTabsView.ErrorType.SYNC_UNAVAILABLE.toSyncedTabsListItem(context, navController)

        assertEquals(testContext.getString(R.string.synced_tabs_connect_another_device), multipleDevicesUnavailable.errorText)
        assertEquals(testContext.getString(R.string.synced_tabs_enable_tab_syncing), syncEngineUnavailable.errorText)
        assertEquals(testContext.getString(R.string.synced_tabs_reauth), syncNeedsReauthentication.errorText)
        assertEquals(testContext.getString(R.string.synced_tabs_no_tabs), noTabsAvailable.errorText)
        assertEquals(testContext.getString(R.string.synced_tabs_sign_in_message), syncUnavailable.errorText)
        assertNotNull(syncUnavailable.errorButton)
        assertEquals(testContext.getString(R.string.synced_tabs_sign_in_button), syncUnavailable.errorButton!!.buttonText)
    }
}

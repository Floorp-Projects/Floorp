package org.mozilla.fenix.sync.ext

import org.junit.Test
import androidx.navigation.NavController
import io.mockk.mockk
import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import org.mozilla.fenix.R
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertEquals

class ErrorTypeKtTest {

    @Test
    fun `string resource for error`() {
        assertEquals(
            R.string.synced_tabs_connect_another_device,
            ErrorType.MULTIPLE_DEVICES_UNAVAILABLE.toStringRes()
        )
        assertEquals(
            R.string.synced_tabs_enable_tab_syncing,
            ErrorType.SYNC_ENGINE_UNAVAILABLE.toStringRes()
        )
        assertEquals(
            R.string.synced_tabs_sign_in_message,
            ErrorType.SYNC_UNAVAILABLE.toStringRes()
        )
        assertEquals(
            R.string.synced_tabs_reauth,
            ErrorType.SYNC_NEEDS_REAUTHENTICATION.toStringRes()
        )
        assertEquals(
            R.string.synced_tabs_no_tabs,
            ErrorType.NO_TABS_AVAILABLE.toStringRes()
        )
    }

    @Test
    fun `get error item`() {
        val navController = mockk<NavController>()

        var errorItem = ErrorType.MULTIPLE_DEVICES_UNAVAILABLE.toAdapterItem(
            R.string.synced_tabs_connect_another_device, navController
        )
        assertNull(errorItem.navController)
        assertEquals(R.string.synced_tabs_connect_another_device, errorItem.descriptionResId)

        errorItem = ErrorType.SYNC_ENGINE_UNAVAILABLE.toAdapterItem(
            R.string.synced_tabs_enable_tab_syncing, navController
        )
        assertNull(errorItem.navController)
        assertEquals(R.string.synced_tabs_enable_tab_syncing, errorItem.descriptionResId)

        errorItem = ErrorType.SYNC_NEEDS_REAUTHENTICATION.toAdapterItem(
            R.string.synced_tabs_reauth, navController
        )
        assertNull(errorItem.navController)
        assertEquals(R.string.synced_tabs_reauth, errorItem.descriptionResId)

        errorItem = ErrorType.NO_TABS_AVAILABLE.toAdapterItem(
            R.string.synced_tabs_no_tabs, navController
        )
        assertNull(errorItem.navController)
        assertEquals(R.string.synced_tabs_no_tabs, errorItem.descriptionResId)

        errorItem = ErrorType.SYNC_UNAVAILABLE.toAdapterItem(
            R.string.synced_tabs_sign_in_message, navController
        )
        assertNotNull(errorItem.navController)
        assertEquals(R.string.synced_tabs_sign_in_message, errorItem.descriptionResId)
    }
}

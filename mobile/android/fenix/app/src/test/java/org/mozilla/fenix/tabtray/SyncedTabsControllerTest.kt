package org.mozilla.fenix.tabtray

import android.view.LayoutInflater
import android.view.View
import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.sync.SyncedTabsViewHolder

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class SyncedTabsControllerTest {

    private lateinit var view: View
    private lateinit var controller: SyncedTabsController

    @Before
    fun setup() = runBlockingTest {
        view = LayoutInflater.from(testContext).inflate(R.layout.about_list_item, null)
        controller = SyncedTabsController(view, coroutineContext)
    }

    @Test
    fun `display synced tabs in reverse`() {
        val tabs = listOf(
            SyncedDeviceTabs(
                device = mockk(relaxed = true),
                tabs = listOf(
                    mockk(relaxed = true),
                    mockk(relaxed = true)
                )
            )
        )

        controller.displaySyncedTabs(tabs)

        val itemCount = controller.adapter.itemCount

        // title + device name + 2 tabs
        assertEquals(4, itemCount)
        assertEquals(
            SyncedTabsViewHolder.TitleViewHolder.LAYOUT_ID,
            controller.adapter.getItemViewType(itemCount - 1)
        )
        assertEquals(
            SyncedTabsViewHolder.DeviceViewHolder.LAYOUT_ID,
            controller.adapter.getItemViewType(itemCount - 2)
        )
        assertEquals(
            SyncedTabsViewHolder.TabViewHolder.LAYOUT_ID,
            controller.adapter.getItemViewType(itemCount - 3)
        )
        assertEquals(
            SyncedTabsViewHolder.TabViewHolder.LAYOUT_ID,
            controller.adapter.getItemViewType(itemCount - 4)
        )
    }

    @Test
    fun `show error when we go kaput`() {
        controller.onError(ErrorType.SYNC_NEEDS_REAUTHENTICATION)

        assertEquals(1, controller.adapter.itemCount)
        assertEquals(
            SyncedTabsViewHolder.ErrorViewHolder.LAYOUT_ID,
            controller.adapter.getItemViewType(0)
        )
    }
}

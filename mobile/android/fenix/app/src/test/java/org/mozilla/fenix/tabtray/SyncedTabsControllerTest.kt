package org.mozilla.fenix.tabtray

import android.view.LayoutInflater
import android.view.View
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.recyclerview.widget.ConcatAdapter
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.storage.sync.SyncedDeviceTabs
import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.sync.SyncedTabsViewHolder
import org.mozilla.fenix.tabtray.TabTrayDialogFragmentAction.EnterMultiSelectMode
import org.mozilla.fenix.tabtray.TabTrayDialogFragmentAction.ExitMultiSelectMode
import org.mozilla.fenix.tabtray.TabTrayDialogFragmentState.Mode

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class SyncedTabsControllerTest {

    private val testDispatcher = TestCoroutineDispatcher()
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    private lateinit var view: View
    private lateinit var controller: SyncedTabsController
    private lateinit var lifecycleOwner: LifecycleOwner
    private lateinit var lifecycle: LifecycleRegistry
    private lateinit var concatAdapter: ConcatAdapter
    private lateinit var store: TabTrayDialogFragmentStore

    @Before
    fun setup() = runBlockingTest {
        lifecycleOwner = mockk()
        lifecycle = LifecycleRegistry(lifecycleOwner)
        lifecycle.handleLifecycleEvent(Lifecycle.Event.ON_START)
        every { lifecycleOwner.lifecycle } returns lifecycle

        concatAdapter = mockk()
        every { concatAdapter.addAdapter(any(), any()) } returns true
        every { concatAdapter.removeAdapter(any()) } returns true

        store = TabTrayDialogFragmentStore(
            initialState = TabTrayDialogFragmentState(
                mode = Mode.Normal,
                browserState = mockk(relaxed = true)
            )
        )

        view = LayoutInflater.from(testContext).inflate(R.layout.about_list_item, null)
        controller =
            SyncedTabsController(lifecycleOwner, view, store, concatAdapter, coroutineContext)
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

    @Test
    fun `do nothing on init, drop first event`() {
        verify { concatAdapter wasNot Called }
    }

    @Test
    fun `concatAdapter updated on mode changes`() = testDispatcher.runBlockingTest {
        store.dispatch(EnterMultiSelectMode).joinBlocking()

        verify { concatAdapter.removeAdapter(any()) }

        store.dispatch(ExitMultiSelectMode).joinBlocking()

        verify { concatAdapter.addAdapter(0, any()) }
    }
}

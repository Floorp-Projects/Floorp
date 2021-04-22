/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.appcompat.content.res.AppCompatResources
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsInteractor
import org.mozilla.fenix.utils.Settings

class FloatingActionButtonBindingTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(TestCoroutineDispatcher())

    private val settings: Settings = mockk(relaxed = true)
    private val actionButton: ExtendedFloatingActionButton = mockk(relaxed = true)
    private val browserTrayInteractor: BrowserTrayInteractor = mockk(relaxed = true)
    private val syncedTabsInteractor: SyncedTabsInteractor = mockk(relaxed = true)

    @Before
    fun setup() {
        mockkStatic(AppCompatResources::class)
        every { AppCompatResources.getDrawable(any(), any()) } returns mockk(relaxed = true)
    }

    @Test
    fun `WHEN tab selected page is normal tab THEN shrink and show is called`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.NormalTabs))
        val fabBinding = FloatingActionButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor, syncedTabsInteractor
        )
        every { settings.accessibilityServicesEnabled } returns false

        fabBinding.start()

        verify(exactly = 1) { actionButton.shrink() }
        verify(exactly = 1) { actionButton.show() }
        verify(exactly = 0) { actionButton.extend() }
        verify(exactly = 0) { actionButton.hide() }
    }

    @Test
    fun `WHEN tab selected page is private tab THEN extend and show is called`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.PrivateTabs))
        val fabBinding = FloatingActionButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor, syncedTabsInteractor
        )
        every { settings.accessibilityServicesEnabled } returns false

        fabBinding.start()

        verify(exactly = 1) { actionButton.extend() }
        verify(exactly = 1) { actionButton.show() }
        verify(exactly = 0) { actionButton.shrink() }
        verify(exactly = 0) { actionButton.hide() }
    }

    @Test
    fun `WHEN tab selected page is sync tab THEN extend and show is called`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.SyncedTabs))
        val fabBinding = FloatingActionButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor, syncedTabsInteractor
        )
        every { settings.accessibilityServicesEnabled } returns false

        fabBinding.start()

        verify(exactly = 1) { actionButton.extend() }
        verify(exactly = 1) { actionButton.show() }
        verify(exactly = 0) { actionButton.shrink() }
        verify(exactly = 0) { actionButton.hide() }
    }

    @Test
    fun `WHEN accessibility is enabled THEN show is not called`() {
        var tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.NormalTabs))
        var fabBinding = FloatingActionButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor, syncedTabsInteractor
        )
        every { settings.accessibilityServicesEnabled } returns true

        fabBinding.start()

        verify(exactly = 0) { actionButton.show() }
        verify(exactly = 1) { actionButton.hide() }

        tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.PrivateTabs))
        fabBinding = FloatingActionButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor, syncedTabsInteractor
        )

        fabBinding.start()

        verify(exactly = 0) { actionButton.show() }
        verify(exactly = 2) { actionButton.hide() }

        tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.SyncedTabs))
        fabBinding = FloatingActionButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor, syncedTabsInteractor
        )

        fabBinding.start()

        verify(exactly = 0) { actionButton.show() }
        verify(exactly = 3) { actionButton.hide() }
    }

    @Test
    fun `WHEN selected page is updated THEN button is updated`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.NormalTabs))
        val fabBinding = FloatingActionButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor, syncedTabsInteractor
        )
        every { settings.accessibilityServicesEnabled } returns false

        fabBinding.start()

        verify(exactly = 1) { actionButton.shrink() }
        verify(exactly = 1) { actionButton.show() }
        verify(exactly = 0) { actionButton.extend() }
        verify(exactly = 0) { actionButton.hide() }
        verify(exactly = 1) { actionButton.setIconResource(R.drawable.ic_new) }

        tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(Page.PrivateTabs.ordinal)))
        tabsTrayStore.waitUntilIdle()

        verify(exactly = 1) { actionButton.shrink() }
        verify(exactly = 2) { actionButton.show() }
        verify(exactly = 1) { actionButton.extend() }
        verify(exactly = 0) { actionButton.hide() }
        verify(exactly = 1) { actionButton.setText(R.string.tab_drawer_fab_content) }
        verify(exactly = 2) { actionButton.setIconResource(R.drawable.ic_new) }

        tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(Page.SyncedTabs.ordinal)))
        tabsTrayStore.waitUntilIdle()

        verify(exactly = 1) { actionButton.shrink() }
        verify(exactly = 3) { actionButton.show() }
        verify(exactly = 2) { actionButton.extend() }
        verify(exactly = 0) { actionButton.hide() }
        verify(exactly = 1) { actionButton.setText(R.string.tab_drawer_fab_sync) }
        verify(exactly = 1) { actionButton.setIconResource(R.drawable.ic_fab_sync) }

        tabsTrayStore.dispatch(TabsTrayAction.SyncNow)
        tabsTrayStore.waitUntilIdle()

        verify(exactly = 1) { actionButton.shrink() }
        verify(exactly = 4) { actionButton.show() }
        verify(exactly = 3) { actionButton.extend() }
        verify(exactly = 0) { actionButton.hide() }
        verify(exactly = 1) { actionButton.setText(R.string.sync_syncing_in_progress) }
        verify(exactly = 2) { actionButton.setIconResource(R.drawable.ic_fab_sync) }
    }
}

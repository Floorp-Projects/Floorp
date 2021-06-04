/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.view.View
import android.widget.ImageButton
import androidx.appcompat.content.res.AppCompatResources
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.utils.Settings

class AccessibleNewTabButtonBindingTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(TestCoroutineDispatcher())

    private val settings: Settings = mockk(relaxed = true)
    private val actionButton: ImageButton = mockk(relaxed = true)
    private val browserTrayInteractor: BrowserTrayInteractor = mockk(relaxed = true)
    private val context: Context = mockk(relaxed = true)

    @Before
    fun setup() {
        mockkStatic(AppCompatResources::class)
        every { AppCompatResources.getDrawable(any(), any()) } returns mockk(relaxed = true)
        every { actionButton.context } returns context
    }

    @After
    fun teardown() {
        unmockkStatic(AppCompatResources::class)
    }

    @Test
    fun `WHEN tab selected page is normal tab THEN new tab button is visible`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.NormalTabs))
        val newTabButtonBinding = AccessibleNewTabButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor
        )
        every { settings.accessibilityServicesEnabled } returns true

        newTabButtonBinding.start()

        verify(exactly = 1) { actionButton.visibility = View.VISIBLE }
    }

    @Test
    fun `WHEN tab selected page is private tab THEN new tab button is visible`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.PrivateTabs))
        val newTabButtonBinding = AccessibleNewTabButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor
        )
        every { settings.accessibilityServicesEnabled } returns true

        newTabButtonBinding.start()

        verify(exactly = 1) { actionButton.visibility = View.VISIBLE }
    }

    @Test
    fun `WHEN tab selected page is sync tab THEN new tab button is visible`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.SyncedTabs))
        val newTabButtonBinding = AccessibleNewTabButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor
        )
        every { settings.accessibilityServicesEnabled } returns true

        newTabButtonBinding.start()

        verify(exactly = 1) { actionButton.visibility = View.VISIBLE }
    }

    @Test
    fun `WHEN accessibility is disabled THEN new tab button is not visible`() {
        var tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.NormalTabs))
        var newTabButtonBinding = AccessibleNewTabButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor
        )
        every { settings.accessibilityServicesEnabled } returns false

        newTabButtonBinding.start()

        verify(exactly = 1) { actionButton.visibility = View.GONE }

        tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.PrivateTabs))
        newTabButtonBinding = AccessibleNewTabButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor
        )

        newTabButtonBinding.start()

        verify(exactly = 2) { actionButton.visibility = View.GONE }

        tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.SyncedTabs))
        newTabButtonBinding = AccessibleNewTabButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor
        )

        newTabButtonBinding.start()

        verify(exactly = 3) { actionButton.visibility = View.GONE }
    }

    @Test
    fun `WHEN selected page is updated THEN button is updated`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.NormalTabs))
        val newTabButtonBinding = AccessibleNewTabButtonBinding(
            tabsTrayStore, settings, actionButton, browserTrayInteractor
        )
        every { settings.accessibilityServicesEnabled } returns true

        newTabButtonBinding.start()

        verify(exactly = 1) { actionButton.setImageResource(R.drawable.ic_new) }
        verify(exactly = 1) { actionButton.contentDescription = any() }

        tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(Page.PrivateTabs.ordinal)))
        tabsTrayStore.waitUntilIdle()

        verify(exactly = 2) { actionButton.setImageResource(R.drawable.ic_new) }
        verify(exactly = 2) { actionButton.contentDescription = any() }

        tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(Page.SyncedTabs.ordinal)))
        tabsTrayStore.waitUntilIdle()

        verify(exactly = 1) { actionButton.setImageResource(R.drawable.ic_fab_sync) }
        verify(exactly = 3) { actionButton.contentDescription = any() }

        tabsTrayStore.dispatch(TabsTrayAction.SyncNow)
        tabsTrayStore.waitUntilIdle()

        verify(exactly = 1) { actionButton.visibility = View.GONE }
    }
}

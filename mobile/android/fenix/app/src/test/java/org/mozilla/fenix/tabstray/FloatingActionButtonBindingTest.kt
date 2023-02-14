/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.appcompat.content.res.AppCompatResources
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.browser.TabsTrayFabInteractor

class FloatingActionButtonBindingTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val actionButton: ExtendedFloatingActionButton = mockk(relaxed = true)
    private val interactor: TabsTrayFabInteractor = mockk(relaxed = true)

    @Before
    fun setup() {
        mockkStatic(AppCompatResources::class)
        every { AppCompatResources.getDrawable(any(), any()) } returns mockk(relaxed = true)
    }

    @After
    fun teardown() {
        unmockkStatic(AppCompatResources::class)
    }

    @Test
    fun `WHEN tab selected page is normal tab THEN shrink and show is called`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.NormalTabs))
        val fabBinding = FloatingActionButtonBinding(
            tabsTrayStore,
            actionButton,
            interactor,
        )

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
            tabsTrayStore,
            actionButton,
            interactor,
        )

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
            tabsTrayStore,
            actionButton,
            interactor,
        )

        fabBinding.start()

        verify(exactly = 1) { actionButton.extend() }
        verify(exactly = 1) { actionButton.show() }
        verify(exactly = 0) { actionButton.shrink() }
        verify(exactly = 0) { actionButton.hide() }
    }

    @Test
    fun `WHEN selected page is updated THEN button is updated`() {
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.NormalTabs))
        val fabBinding = FloatingActionButtonBinding(
            tabsTrayStore,
            actionButton,
            interactor,
        )

        fabBinding.start()

        verify(exactly = 1) { actionButton.shrink() }
        verify(exactly = 1) { actionButton.show() }
        verify(exactly = 0) { actionButton.extend() }
        verify(exactly = 0) { actionButton.hide() }
        verify(exactly = 1) { actionButton.setIconResource(R.drawable.ic_new) }
        verify(exactly = 1) { actionButton.contentDescription = any() }

        tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(Page.PrivateTabs.ordinal)))
        tabsTrayStore.waitUntilIdle()

        verify(exactly = 1) { actionButton.shrink() }
        verify(exactly = 2) { actionButton.show() }
        verify(exactly = 1) { actionButton.extend() }
        verify(exactly = 0) { actionButton.hide() }
        verify(exactly = 1) { actionButton.setText(R.string.tab_drawer_fab_content) }
        verify(exactly = 2) { actionButton.setIconResource(R.drawable.ic_new) }
        verify(exactly = 2) { actionButton.contentDescription = any() }

        tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(Page.SyncedTabs.ordinal)))
        tabsTrayStore.waitUntilIdle()

        verify(exactly = 1) { actionButton.shrink() }
        verify(exactly = 3) { actionButton.show() }
        verify(exactly = 2) { actionButton.extend() }
        verify(exactly = 0) { actionButton.hide() }
        verify(exactly = 1) { actionButton.setText(R.string.tab_drawer_fab_sync) }
        verify(exactly = 1) { actionButton.setIconResource(R.drawable.ic_fab_sync) }
        verify(exactly = 3) { actionButton.contentDescription = any() }

        tabsTrayStore.dispatch(TabsTrayAction.SyncNow)
        tabsTrayStore.waitUntilIdle()

        verify(exactly = 1) { actionButton.shrink() }
        verify(exactly = 4) { actionButton.show() }
        verify(exactly = 3) { actionButton.extend() }
        verify(exactly = 0) { actionButton.hide() }
        verify(exactly = 1) { actionButton.setText(R.string.sync_syncing_in_progress) }
        verify(exactly = 2) { actionButton.setIconResource(R.drawable.ic_fab_sync) }
        verify(exactly = 4) { actionButton.contentDescription = any() }
    }
}

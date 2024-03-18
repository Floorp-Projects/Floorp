/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayStore

class SelectedItemAdapterBindingTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val adapter = mockk<BrowserTabsAdapter>(relaxed = true)

    @Before
    fun setup() {
        every { adapter.itemCount }.answers { 1 }
    }

    @Test
    fun `WHEN mode changes THEN notify the adapter`() {
        val store = TabsTrayStore()
        val binding = SelectedItemAdapterBinding(store, adapter)

        binding.start()

        store.dispatch(TabsTrayAction.EnterSelectMode)

        store.waitUntilIdle()

        verify {
            adapter.notifyItemRangeChanged(eq(0), eq(1), eq(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM))
        }

        store.dispatch(TabsTrayAction.ExitSelectMode)

        store.waitUntilIdle()

        verify {
            adapter.notifyItemRangeChanged(eq(0), eq(1), eq(PAYLOAD_HIGHLIGHT_SELECTED_ITEM))
        }
    }
}

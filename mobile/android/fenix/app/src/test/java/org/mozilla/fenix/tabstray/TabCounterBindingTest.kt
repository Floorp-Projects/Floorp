/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.ui.tabcounter.TabCounter
import org.junit.Rule
import org.junit.Test

class TabCounterBindingTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN normalTabs changes THEN update counter`() {
        val store = BrowserStore()
        val counter = mockk<TabCounter>(relaxed = true)
        val binding = TabCounterBinding(store, counter)

        binding.start()

        store.dispatch(TabListAction.AddTabAction(createTab("https://mozilla.org")))

        store.waitUntilIdle()

        verify { counter.setCount(1) }
    }

    @Test
    fun `WHEN feature starts THEN update counter`() {
        val store = BrowserStore()
        val counter = mockk<TabCounter>(relaxed = true)
        val binding = TabCounterBinding(store, counter)

        binding.start()

        store.waitUntilIdle()

        verify { counter.setCount(0) }
    }
}

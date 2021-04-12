/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

class AbstractBindingTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(TestCoroutineDispatcher())

    @Test
    fun `WHEN started THEN onState flow is invoked`() {
        val store = BrowserStore()
        var invoked = false
        val binding = TestBinding(store) {
            invoked = true
        }

        binding.start()

        store.waitUntilIdle()

        assertTrue(invoked)
    }

    @Test
    fun `WHEN actions are dispatched THEN onState flow is invoked`() {
        val store = BrowserStore()
        var invoked = false
        val binding = TestBinding(store) {
            if (store.state.tabs.isNotEmpty()) {
                invoked = true
            }
        }

        binding.start()

        store.dispatch(TabListAction.AddTabAction(createTab("https://mozilla.org")))

        store.waitUntilIdle()

        assertTrue(invoked)
    }

    class TestBinding(
        store: BrowserStore,
        private val invoked: (BrowserState) -> Unit
    ) : AbstractBinding<BrowserState>(store) {
        override suspend fun onState(flow: Flow<BrowserState>) {
            flow.collect {
                invoked(it)
            }
        }
    }
}

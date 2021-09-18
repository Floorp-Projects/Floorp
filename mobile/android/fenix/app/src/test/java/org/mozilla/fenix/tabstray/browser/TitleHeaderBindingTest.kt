/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test

@ExperimentalCoroutinesApi
class TitleHeaderBindingTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN normal tabs are added to the list THEN return true`() = runBlockingTest {
        var result = false
        val store = BrowserStore()
        val binding = TitleHeaderBinding(store) { result = it }

        store.dispatch(TabListAction.AddTabAction(createTab("https://mozilla.org"))).joinBlocking()

        binding.start()

        store.waitUntilIdle()

        assertTrue(result)
    }

    @Test
    fun `WHEN grouped tabs are added to the list THEN return false`() = runBlockingTest {
        var result = false
        val store = BrowserStore()
        val binding = TitleHeaderBinding(store) { result = it }

        store.dispatch(
            TabListAction.AddTabAction(
                createTab(
                    url = "https://mozilla.org",
                    historyMetadata = HistoryMetadataKey(
                        url = "https://getpocket.com",
                        searchTerm = "Mozilla"
                    )
                )
            )
        ).joinBlocking()

        binding.start()

        store.waitUntilIdle()

        assertFalse(result)
    }

    @Test
    fun `WHEN normal tabs are all removed THEN return false`() = runBlockingTest {
        var result = false
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(createTab("https://getpocket.com", id = "123"))
            )
        )
        val binding = TitleHeaderBinding(store) { result = it }

        store.dispatch(TabListAction.RemoveTabAction("123")).joinBlocking()

        binding.start()

        store.waitUntilIdle()

        assertFalse(result)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayStore

class TabGroupBindingTest {
    val store = TabsTrayStore()
    var captured: List<TabGroup>? = null
    val binding = TabGroupBinding(store) { captured = it }

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @After
    fun teardown() {
        binding.stop()
    }

    @Test
    fun `WHEN the store is updated THEN notify the adapter`() {
        val expectedGroups = listOf(TabGroup("cats", emptyList(), 0))

        assertTrue(store.state.searchTermGroups.isEmpty())

        store.dispatch(TabsTrayAction.UpdateSearchGroupTabs(expectedGroups)).joinBlocking()

        binding.start()

        assertTrue(store.state.searchTermGroups.isNotEmpty())

        assertEquals(expectedGroups, captured)
    }

    @Test
    fun `WHEN non-group tabs are updated THEN do not notify the adapter`() {
        assertTrue(store.state.searchTermGroups.isEmpty())

        store.dispatch(TabsTrayAction.UpdatePrivateTabs(listOf(createTab("https://mozilla.org")))).joinBlocking()

        binding.start()

        assertTrue(store.state.searchTermGroups.isEmpty())

        assertEquals(emptyList<TabGroup>(), captured)
    }
}

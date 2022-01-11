/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.tabstray.SEARCH_TERM_TAB_GROUPS
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
        val expectedTabGroups = listOf(TabGroup("cats", "name", listOf("1", "2")))
        val tabPartition = TabPartition(SEARCH_TERM_TAB_GROUPS, expectedTabGroups)

        assertNull(store.state.searchTermPartition?.tabGroups)

        store.dispatch(TabsTrayAction.UpdateTabPartitions(tabPartition)).joinBlocking()

        binding.start()

        assertTrue(store.state.searchTermPartition?.tabGroups?.isNotEmpty() == true)

        assertEquals(expectedTabGroups, captured)
    }

    @Test
    fun `WHEN the store is updated with empty tab group THEN notify the adapter`() {
        val expectedTabPartition = TabPartition(SEARCH_TERM_TAB_GROUPS, listOf(TabGroup("cats", "name", emptyList())))

        assertNull(store.state.searchTermPartition?.tabGroups)

        store.dispatch(TabsTrayAction.UpdateTabPartitions(expectedTabPartition)).joinBlocking()

        binding.start()

        assertTrue(store.state.searchTermPartition?.tabGroups?.isNotEmpty() == true)

        assertEquals(emptyList<TabGroup>(), captured)
    }

    @Test
    fun `WHEN non-group tabs are updated THEN do not notify the adapter`() {
        assertEquals(store.state.searchTermPartition?.tabGroups, null)

        store.dispatch(TabsTrayAction.UpdatePrivateTabs(listOf(createTab("https://mozilla.org")))).joinBlocking()

        binding.start()

        assertNull(store.state.searchTermPartition?.tabGroups)

        assertEquals(emptyList<TabGroup>(), captured)
    }
}

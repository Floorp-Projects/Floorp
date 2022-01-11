/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.tabstray.SEARCH_TERM_TAB_GROUPS
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

class OtherHeaderBindingTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN there are no tabs THEN show no header`() {
        val store = TabsTrayStore()
        var result: Boolean? = null
        val binding = OtherHeaderBinding(store) { result = it }

        binding.start()

        store.waitUntilIdle()

        assertFalse(result!!)
    }

    @Test
    fun `WHEN tabs for only groups THEN show no header`() {
        val store = TabsTrayStore(TabsTrayState(searchTermPartition = mockk()))
        var result: Boolean? = null
        val binding = OtherHeaderBinding(store) { result = it }

        binding.start()

        store.waitUntilIdle()

        assertFalse(result!!)
    }

    @Test
    fun `WHEN tabs for only normal tabs THEN show no header`() {
        val store = TabsTrayStore(TabsTrayState(normalTabs = listOf(mockk())))
        var result: Boolean? = null
        val binding = OtherHeaderBinding(store) { result = it }

        binding.start()

        store.waitUntilIdle()

        assertFalse(result!!)
    }

    @Test
    fun `WHEN normal tabs and groups exist THEN show header`() {
        val tabGroup = TabGroup("test", "", listOf("1", "2"))
        val store = TabsTrayStore(
            TabsTrayState(
                normalTabs = listOf(mockk()),
                searchTermPartition = TabPartition(SEARCH_TERM_TAB_GROUPS, listOf(tabGroup))
            )
        )
        var result = false
        val binding = OtherHeaderBinding(store) { result = it }

        binding.start()

        store.waitUntilIdle()

        assertTrue(result)
    }
}

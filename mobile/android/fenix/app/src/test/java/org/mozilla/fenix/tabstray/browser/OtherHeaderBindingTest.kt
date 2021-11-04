/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
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
        val store = TabsTrayStore(TabsTrayState(searchTermGroups = listOf(mockk())))
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
        val store = TabsTrayStore(TabsTrayState(normalTabs = listOf(mockk()), searchTermGroups = listOf(mockk())))
        var result: Boolean? = null
        val binding = OtherHeaderBinding(store) { result = it }

        binding.start()

        store.waitUntilIdle()

        assertTrue(result!!)
    }
}

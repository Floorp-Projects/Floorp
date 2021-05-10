/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.syncedtabs

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayStore

class SyncButtonBindingTest {
    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(TestCoroutineDispatcher())

    @Test
    fun `WHEN syncing state is true THEN invoke callback`() {
        var invoked = false
        val store = TabsTrayStore()
        val binding = SyncButtonBinding(store) { invoked = true }

        binding.start()

        store.dispatch(TabsTrayAction.SyncNow)
        store.waitUntilIdle()

        assertTrue(invoked)
    }

    @Test
    fun `WHEN syncing state is false THEN nothing is invoked`() {
        var invoked = false
        val store = TabsTrayStore()
        val binding = SyncButtonBinding(store) { invoked = true }

        binding.start()

        store.waitUntilIdle()

        assertFalse(invoked)

        store.dispatch(TabsTrayAction.SyncCompleted)
        store.waitUntilIdle()

        assertFalse(invoked)
    }
}

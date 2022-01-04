/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

class SwipeToDeleteBindingTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN started THEN update the swipeable state`() {
        val store = TabsTrayStore(TabsTrayState(mode = TabsTrayState.Mode.Select(emptySet())))
        val binding = SwipeToDeleteBinding(store)

        binding.start()

        assertFalse(binding.isSwipeable)

        store.dispatch(TabsTrayAction.ExitSelectMode)

        store.waitUntilIdle()

        assertTrue(binding.isSwipeable)
    }

    @Test
    fun `default state of binding is false`() {
        val binding = SwipeToDeleteBinding(mockk())

        assertFalse(binding.isSwipeable)
    }
}

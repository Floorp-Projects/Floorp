/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state.bindings

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore

class MenuBindingTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN the mode is updated THEN the menu is invalidated`() {
        var menuInvalidated = false
        val store = HistoryFragmentStore(HistoryFragmentState.initial.copy(mode = HistoryFragmentState.Mode.Syncing))
        val binding = MenuBinding(
            store = store,
            invalidateOptionsMenu = { menuInvalidated = true },
        )

        binding.start()
        store.dispatch(HistoryFragmentAction.FinishSync).joinBlocking()

        assertTrue(menuInvalidated)
    }
}

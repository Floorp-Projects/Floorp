/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore

class MenuStoreTest {

    private lateinit var state: MenuState
    private lateinit var store: MenuStore

    @Before
    fun setup() {
        state = MenuState()
        store = MenuStore(initialState = state)
    }

    @Test
    fun `WHEN update bookmarked action is dispatched THEN bookmarked state is updated`() = runTest {
        assertFalse(store.state.isBookmarked)

        store.dispatch(MenuAction.UpdateBookmarked(isBookmarked = true)).join()
        assertTrue(store.state.isBookmarked)

        store.dispatch(MenuAction.UpdateBookmarked(isBookmarked = false)).join()
        assertFalse(store.state.isBookmarked)
    }
}

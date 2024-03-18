/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.AppStore

class TabStripActionTest {

    @Test
    fun `WHEN the last remaining tab that was closed was private THEN state should reflect that`() {
        val store = AppStore(initialState = AppState())

        store.dispatch(AppAction.TabStripAction.UpdateLastTabClosed(true)).joinBlocking()

        val expected = AppState(wasLastTabClosedPrivate = true)

        assertEquals(expected, store.state)
    }

    @Test
    fun `WHEN the last remaining tab that was closed was not private THEN state should reflect that`() {
        val store = AppStore(initialState = AppState())

        store.dispatch(AppAction.TabStripAction.UpdateLastTabClosed(false)).joinBlocking()

        val expected = AppState(wasLastTabClosedPrivate = false)

        assertEquals(expected, store.state)
    }
}

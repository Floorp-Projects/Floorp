/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import org.junit.Assert.assertEquals
import org.junit.Test

class TabsTrayStoreReducerTest {
    @Test
    fun `GIVEN focusGroupTabId WHEN ConsumeFocusGroupTabIdAction THEN focusGroupTabId must be consumed`() {
        val initialState = TabsTrayState(focusGroupTabId = "id")
        val expectedState = initialState.copy(focusGroupTabId = null)

        val resultState = TabsTrayReducer.reduce(
            initialState,
            TabsTrayAction.ConsumeFocusGroupTabIdAction
        )

        assertEquals(expectedState, resultState)
    }
}

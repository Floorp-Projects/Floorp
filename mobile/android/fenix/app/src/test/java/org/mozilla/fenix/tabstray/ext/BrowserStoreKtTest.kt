/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import io.mockk.mockk
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.Tab
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.tabstray.browser.createTab

class BrowserStoreKtTest {

    @Test
    fun `WHEN session is found THEN return it`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    TabSessionState(id = "tab1", mockk(), lastAccess = 3),
                    TabSessionState(id = "tab2", mockk(), lastAccess = 5)
                )
            )
        )

        val tabs = listOf<Tab>(
            createTab("tab1"),
            createTab("tab2")
        )

        val result = store.getTabSessionState(tabs)

        assertEquals(3, result[0].lastAccess)
        assertEquals(5, result[1].lastAccess)
    }

    @Test
    fun `WHEN session is not found THEN ignore it`() {
        val store = BrowserStore(
            initialState = BrowserState(
                listOf(
                    TabSessionState(id = "tab2", mockk(), lastAccess = 5)
                )
            )
        )

        val tabs = listOf<Tab>(
            createTab("tab1"),
            createTab("tab2")
        )

        val result = store.getTabSessionState(tabs)

        assertEquals(5, result[0].lastAccess)
        assertEquals(1, result.size)
    }
}

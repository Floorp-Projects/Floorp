/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Before

class ShoppingProductActionTest {
    private lateinit var tab: TabSessionState
    private lateinit var store: BrowserStore

    @Before
    fun setUp() {
        tab = createTab("https://www.mozilla.org")

        store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )
    }

    private fun tabState(): TabSessionState = store.state.findTab(tab.id)!!

    // TO DO: add tests after a test endpoint is implemented in desktop (Bug 1846341)
}

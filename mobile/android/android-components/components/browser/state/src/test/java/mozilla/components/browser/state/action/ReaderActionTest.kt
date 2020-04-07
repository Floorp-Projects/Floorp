/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class ReaderActionTest {
    private lateinit var tab: TabSessionState
    private lateinit var store: BrowserStore

    @Before
    fun setUp() {
        tab = createTab("https://www.mozilla.org")

        store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab)
            )
        )
    }

    private fun tabState(): TabSessionState = store.state.findTab(tab.id)!!
    private fun readerState() = tabState().readerState

    @Test
    fun `UpdateReaderableAction - Updates readerable flag of ReaderState`() {
        assertFalse(readerState().readerable)

        store.dispatch(ReaderAction.UpdateReaderableAction(tabId = tab.id, readerable = true))
            .joinBlocking()

        assertTrue(readerState().readerable)

        store.dispatch(ReaderAction.UpdateReaderableAction(tabId = tab.id, readerable = false))
            .joinBlocking()

        assertFalse(readerState().readerable)
    }

    @Test
    fun `UpdateReaderActiveAction - Updates active flag of ReaderState`() {
        assertFalse(readerState().active)

        store.dispatch(ReaderAction.UpdateReaderActiveAction(tabId = tab.id, active = true))
            .joinBlocking()

        assertTrue(readerState().active)

        store.dispatch(ReaderAction.UpdateReaderActiveAction(tabId = tab.id, active = false))
            .joinBlocking()

        assertFalse(readerState().active)
    }

    @Test
    fun `UpdateReaderableCheckRequiredAction - Updates check required flag of ReaderState`() {
        assertFalse(readerState().active)

        store.dispatch(ReaderAction.UpdateReaderableCheckRequiredAction(tabId = tab.id, checkRequired = true))
            .joinBlocking()

        assertTrue(readerState().checkRequired)

        store.dispatch(ReaderAction.UpdateReaderableCheckRequiredAction(tabId = tab.id, checkRequired = false))
            .joinBlocking()

        assertFalse(readerState().checkRequired)
    }

    @Test
    fun `UpdateReaderConnectRequiredAction - Updates connect required flag of ReaderState`() {
        assertFalse(readerState().active)

        store.dispatch(ReaderAction.UpdateReaderConnectRequiredAction(tabId = tab.id, connectRequired = true))
            .joinBlocking()

        assertTrue(readerState().connectRequired)

        store.dispatch(ReaderAction.UpdateReaderConnectRequiredAction(tabId = tab.id, connectRequired = false))
            .joinBlocking()

        assertFalse(readerState().connectRequired)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class HistoryMetadataActionTest {

    @Test
    fun `SetHistoryMetadataKeyAction - Associates tab with history metadata`() {
        val tab = createTab("https://www.mozilla.org")
        val store = BrowserStore(BrowserState(tabs = listOf(tab)))
        assertNull(store.state.findTab(tab.id)?.historyMetadata)

        val historyMetadata = HistoryMetadataKey(
            url = tab.content.url,
            referrerUrl = "https://firefox.com"
        )

        store.dispatch(HistoryMetadataAction.SetHistoryMetadataKeyAction(tab.id, historyMetadata)).joinBlocking()
        assertEquals(historyMetadata, store.state.findTab(tab.id)?.historyMetadata)
    }
}

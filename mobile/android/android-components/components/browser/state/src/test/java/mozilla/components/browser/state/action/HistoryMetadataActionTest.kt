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
            referrerUrl = "https://firefox.com",
        )

        store.dispatch(HistoryMetadataAction.SetHistoryMetadataKeyAction(tab.id, historyMetadata)).joinBlocking()
        assertEquals(historyMetadata, store.state.findTab(tab.id)?.historyMetadata)
    }

    @Test
    fun `DisbandSearchGroupAction - clears specific search terms from any existing tab history metadata`() {
        val tab1 = createTab("https://www.mozilla.org")
        val tab2 = createTab("https://www.mozilla.org/downloads")
        val store = BrowserStore(BrowserState(tabs = listOf(tab1, tab2)))
        val historyMetadata1 = HistoryMetadataKey(
            url = tab1.content.url,
            referrerUrl = "https://firefox.com",
        )
        val historyMetadata2 = HistoryMetadataKey(
            url = tab2.content.url,
            searchTerm = "download Firefox",
            referrerUrl = "https://google.com/?q=download+firefox",
        )

        // Okay to do this without any metadata associated with tabs.
        store.dispatch(HistoryMetadataAction.DisbandSearchGroupAction("Download firefox")).joinBlocking()

        // Okay to do this with an empty search term string.
        store.dispatch(HistoryMetadataAction.DisbandSearchGroupAction("")).joinBlocking()

        store.dispatch(HistoryMetadataAction.SetHistoryMetadataKeyAction(tab1.id, historyMetadata1)).joinBlocking()
        store.dispatch(HistoryMetadataAction.SetHistoryMetadataKeyAction(tab2.id, historyMetadata2)).joinBlocking()

        // Search term matching is case-insensitive.
        store.dispatch(HistoryMetadataAction.DisbandSearchGroupAction("Download firefox")).joinBlocking()

        // tab1 is unchanged.
        assertEquals(historyMetadata1, store.state.findTab(tab1.id)?.historyMetadata)

        // tab2 has its search term and referrer cleared.
        assertEquals(HistoryMetadataKey(url = tab2.content.url), store.state.findTab(tab2.id)?.historyMetadata)
    }
}

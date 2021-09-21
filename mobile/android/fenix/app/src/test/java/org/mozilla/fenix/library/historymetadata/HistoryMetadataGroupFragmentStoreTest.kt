/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.historymetadata

import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.HistoryMetadataKey
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.library.history.History

class HistoryMetadataGroupFragmentStoreTest {

    private lateinit var state: HistoryMetadataGroupFragmentState
    private lateinit var store: HistoryMetadataGroupFragmentStore

    private val mozillaHistoryMetadataItem = History.Metadata(
        id = 0,
        title = "Mozilla",
        url = "mozilla.org",
        visitedAt = 0,
        totalViewTime = 0,
        historyMetadataKey = HistoryMetadataKey("http://www.mozilla.com", "mozilla", null)
    )
    private val firefoxHistoryMetadataItem = History.Metadata(
        id = 0,
        title = "Firefox",
        url = "firefox.com",
        visitedAt = 0,
        totalViewTime = 0,
        historyMetadataKey = HistoryMetadataKey("http://www.firefox.com", "mozilla", null)
    )

    @Before
    fun setup() {
        state = HistoryMetadataGroupFragmentState()
        store = HistoryMetadataGroupFragmentStore(state)
    }

    @Test
    fun `Test updating the items in HistoryMetadataGroupFragmentStore`() = runBlocking {
        assertEquals(0, store.state.items.size)

        val items = listOf(mozillaHistoryMetadataItem, firefoxHistoryMetadataItem)
        store.dispatch(HistoryMetadataGroupFragmentAction.UpdateHistoryItems(items)).join()

        assertEquals(items, store.state.items)
    }

    @Test
    fun `Test selecting and deselecting an item in HistoryMetadataGroupFragmentStore`() = runBlocking {
        val items = listOf(mozillaHistoryMetadataItem, firefoxHistoryMetadataItem)

        store.dispatch(HistoryMetadataGroupFragmentAction.UpdateHistoryItems(items)).join()

        assertFalse(store.state.items[0].selected)
        assertFalse(store.state.items[1].selected)

        store.dispatch(HistoryMetadataGroupFragmentAction.Select(mozillaHistoryMetadataItem)).join()

        assertTrue(store.state.items[0].selected)
        assertFalse(store.state.items[1].selected)

        store.dispatch(HistoryMetadataGroupFragmentAction.Deselect(store.state.items[0])).join()

        assertFalse(store.state.items[0].selected)
        assertFalse(store.state.items[1].selected)
    }

    @Test
    fun `Test deselecting all items in HistoryMetadataGroupFragmentStore`() = runBlocking {
        val items = listOf(mozillaHistoryMetadataItem, firefoxHistoryMetadataItem)

        store.dispatch(HistoryMetadataGroupFragmentAction.UpdateHistoryItems(items)).join()
        store.dispatch(HistoryMetadataGroupFragmentAction.Select(mozillaHistoryMetadataItem)).join()
        store.dispatch(HistoryMetadataGroupFragmentAction.DeselectAll).join()

        assertFalse(store.state.items[0].selected)
        assertFalse(store.state.items[1].selected)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class DownloadActionTest {

    @Test
    fun `QueueDownloadAction adds download`() {
        val store = BrowserStore(BrowserState())

        val download1 = DownloadState("https://mozilla.org/download1", destinationDirectory = "")
        store.dispatch(DownloadAction.QueueDownloadAction(download1)).joinBlocking()
        assertEquals(download1, store.state.queuedDownloads[download1.id])
        assertEquals(1, store.state.queuedDownloads.size)

        val download2 = DownloadState("https://mozilla.org/download2", destinationDirectory = "")
        store.dispatch(DownloadAction.QueueDownloadAction(download2)).joinBlocking()
        assertEquals(download2, store.state.queuedDownloads[download2.id])
        assertEquals(2, store.state.queuedDownloads.size)
    }

    @Test
    fun `RemoveQueuedDownloadAction removes download`() {
        val store = BrowserStore(BrowserState())

        val download = DownloadState("https://mozilla.org/download1", destinationDirectory = "")
        store.dispatch(DownloadAction.QueueDownloadAction(download)).joinBlocking()
        assertEquals(download, store.state.queuedDownloads[download.id])
        assertFalse(store.state.queuedDownloads.isEmpty())

        store.dispatch(DownloadAction.RemoveQueuedDownloadAction(download.id)).joinBlocking()
        assertTrue(store.state.queuedDownloads.isEmpty())
    }

    @Test
    fun `RemoveAllQueuedDownloadsAction removes all downloads`() {
        val store = BrowserStore(BrowserState())

        val download = DownloadState("https://mozilla.org/download1", destinationDirectory = "")
        val download2 = DownloadState("https://mozilla.org/download2", destinationDirectory = "")
        store.dispatch(DownloadAction.QueueDownloadAction(download)).joinBlocking()
        store.dispatch(DownloadAction.QueueDownloadAction(download2)).joinBlocking()

        assertFalse(store.state.queuedDownloads.isEmpty())
        assertEquals(2, store.state.queuedDownloads.size)

        store.dispatch(DownloadAction.RemoveAllQueuedDownloadsAction).joinBlocking()
        assertTrue(store.state.queuedDownloads.isEmpty())
    }
}

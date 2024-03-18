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
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class DownloadActionTest {

    @Test
    fun `AddDownloadAction adds download`() {
        val store = BrowserStore(BrowserState())

        val download1 = DownloadState("https://mozilla.org/download1", destinationDirectory = "")
        store.dispatch(DownloadAction.AddDownloadAction(download1)).joinBlocking()
        assertEquals(download1, store.state.downloads[download1.id])
        assertEquals(1, store.state.downloads.size)

        val download2 = DownloadState("https://mozilla.org/download2", destinationDirectory = "")
        store.dispatch(DownloadAction.AddDownloadAction(download2)).joinBlocking()
        assertEquals(download2, store.state.downloads[download2.id])
        assertEquals(2, store.state.downloads.size)
    }

    @Test
    fun `WHEN DismissDownloadNotificationAction is dispatched THEN notificationId is set to null`() {
        val store = BrowserStore(BrowserState())

        val download = DownloadState("https://mozilla.org/download1", destinationDirectory = "", notificationId = 100)
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        assertNotNull(store.state.downloads[download.id]!!.notificationId)

        store.dispatch(DownloadAction.DismissDownloadNotificationAction(download.id)).joinBlocking()
        assertNull(store.state.downloads[download.id]!!.notificationId)
    }

    @Test
    fun `WHEN DismissDownloadNotificationAction is dispatched with an invalid downloadId THEN the state must not change`() {
        val store = BrowserStore(BrowserState())

        val download = DownloadState("https://mozilla.org/download1", destinationDirectory = "", notificationId = 100)
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        assertNotNull(store.state.downloads[download.id]!!.notificationId)
        assertEquals(1, store.state.downloads.size)

        store.dispatch(DownloadAction.DismissDownloadNotificationAction("-1")).joinBlocking()
        assertNotNull(store.state.downloads[download.id]!!.notificationId)
        assertEquals(download, store.state.downloads[download.id])
    }

    @Test
    fun `RestoreDownloadStateAction adds download`() {
        val store = BrowserStore(BrowserState())

        val download1 = DownloadState("https://mozilla.org/download1", destinationDirectory = "")
        store.dispatch(DownloadAction.RestoreDownloadStateAction(download1)).joinBlocking()
        assertEquals(download1, store.state.downloads[download1.id])
        assertEquals(1, store.state.downloads.size)

        val download2 = DownloadState("https://mozilla.org/download2", destinationDirectory = "")
        store.dispatch(DownloadAction.RestoreDownloadStateAction(download2)).joinBlocking()
        assertEquals(download2, store.state.downloads[download2.id])
        assertEquals(2, store.state.downloads.size)
    }

    @Test
    fun `RestoreDownloadsStateAction does nothing`() {
        val store = BrowserStore(BrowserState())

        val state = store.state
        store.dispatch(DownloadAction.RestoreDownloadsStateAction).joinBlocking()
        assertSame(store.state, state)
    }

    @Test
    fun `RemoveDownloadAction removes download`() {
        val store = BrowserStore(BrowserState())

        val download = DownloadState("https://mozilla.org/download1", destinationDirectory = "")
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        assertEquals(download, store.state.downloads[download.id])
        assertFalse(store.state.downloads.isEmpty())

        store.dispatch(DownloadAction.RemoveDownloadAction(download.id)).joinBlocking()
        assertTrue(store.state.downloads.isEmpty())
    }

    @Test
    fun `RemoveAllDownloadsAction removes all downloads`() {
        val store = BrowserStore(BrowserState())

        val download = DownloadState("https://mozilla.org/download1", destinationDirectory = "")
        val download2 = DownloadState("https://mozilla.org/download2", destinationDirectory = "")
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        store.dispatch(DownloadAction.AddDownloadAction(download2)).joinBlocking()

        assertFalse(store.state.downloads.isEmpty())
        assertEquals(2, store.state.downloads.size)

        store.dispatch(DownloadAction.RemoveAllDownloadsAction).joinBlocking()
        assertTrue(store.state.downloads.isEmpty())
    }

    @Test
    fun `UpdateDownloadAction updates the provided download`() {
        val store = BrowserStore(BrowserState())
        val download = DownloadState("https://mozilla.org/download1", destinationDirectory = "")
        val download2 = DownloadState("https://mozilla.org/download2", destinationDirectory = "")

        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        store.dispatch(DownloadAction.AddDownloadAction(download2)).joinBlocking()

        val updatedDownload = download.copy(fileName = "filename.txt")

        store.dispatch(DownloadAction.UpdateDownloadAction(updatedDownload)).joinBlocking()

        assertFalse(store.state.downloads.isEmpty())
        assertEquals(2, store.state.downloads.size)
        assertEquals(updatedDownload, store.state.downloads[updatedDownload.id])
    }
}

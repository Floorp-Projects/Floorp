/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.manager

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.app.DownloadManager.ACTION_DOWNLOAD_COMPLETE
import android.app.DownloadManager.Request
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.EXTRA_DOWNLOAD_STATUS
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions

@RunWith(AndroidJUnit4::class)
class AndroidDownloadManagerTest {

    private lateinit var store: BrowserStore
    private lateinit var download: DownloadState
    private lateinit var downloadManager: AndroidDownloadManager

    @Before
    fun setup() {
        download = DownloadState(
            "http://ipv4.download.thinkbroadband.com/5MB.zip",
            "", "application/zip", 5242880,
            userAgent = "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36"
        )
        store = BrowserStore()
        downloadManager = AndroidDownloadManager(testContext, store)
    }

    @Test(expected = SecurityException::class)
    fun `calling download without the right permission must throw an exception`() {
        downloadManager.download(download)
    }

    @Test
    fun `calling download must download the file`() {
        var downloadCompleted = false

        downloadManager.onDownloadStopped = { _, _, _ -> downloadCompleted = true }

        grantPermissions()

        assertTrue(store.state.downloads.isEmpty())
        val id = downloadManager.download(download)!!
        store.waitUntilIdle()
        assertEquals(download.copy(id = id), store.state.downloads[id])

        notifyDownloadCompleted(id)
        assertTrue(downloadCompleted)
    }

    @Test
    fun `calling tryAgain starts the download again`() {
        var downloadStopped = false

        downloadManager.onDownloadStopped = { _, _, _ -> downloadStopped = true }
        grantPermissions()

        val id = downloadManager.download(download)!!
        store.waitUntilIdle()
        notifyDownloadFailed(id)
        assertTrue(downloadStopped)

        downloadStopped = false
        downloadManager.tryAgain(id)
        notifyDownloadCompleted(id)
        assertTrue(downloadStopped)
    }

    @Test
    fun `trying to download a file with invalid protocol must NOT triggered a download`() {
        val invalidDownload = download.copy(url = "ftp://ipv4.download.thinkbroadband.com/5MB.zip")
        grantPermissions()

        val id = downloadManager.download(invalidDownload)
        assertNull(id)
    }

    @Test
    fun `sendBroadcast with valid downloadID must call onDownloadStopped after download`() {
        var downloadCompleted = false
        var downloadStatus: DownloadState.Status? = null
        val downloadWithFileName = download.copy(fileName = "5MB.zip")

        grantPermissions()

        val id = downloadManager.download(
            downloadWithFileName,
            cookie = "yummy_cookie=choco"
        )!!
        store.waitUntilIdle()

        downloadManager.onDownloadStopped = { _, _, status ->
            downloadStatus = status
            downloadCompleted = true
        }

        notifyDownloadCompleted(id)

        assertTrue(downloadCompleted)
        assertEquals(DownloadState.Status.COMPLETED, downloadStatus)
    }

    @Test
    fun `sendBroadcast with completed download`() {
        var downloadStatus: DownloadState.Status? = null
        val downloadWithFileName = download.copy(fileName = "5MB.zip")
        grantPermissions()

        downloadManager.onDownloadStopped = { _, _, status ->
            downloadStatus = status
        }

        val id = downloadManager.download(
            downloadWithFileName,
            cookie = "yummy_cookie=choco"
        )!!
        store.waitUntilIdle()
        assertEquals(downloadWithFileName.copy(id = id), store.state.downloads[id])

        notifyDownloadCompleted(id)
        store.waitUntilIdle()
        assertEquals(DownloadState.Status.COMPLETED, downloadStatus)
    }

    @Test
    fun `no null or empty headers can be added to the DownloadManager`() {
        val mockRequest: Request = mock()

        mockRequest.addRequestHeaderSafely("User-Agent", "")

        verifyNoInteractions(mockRequest)

        mockRequest.addRequestHeaderSafely("User-Agent", null)

        verifyNoInteractions(mockRequest)

        val fireFox = "Mozilla/5.0 (Windows NT 5.1; rv:7.0.1) Gecko/20100101 Firefox/7.0.1"

        mockRequest.addRequestHeaderSafely("User-Agent", fireFox)

        verify(mockRequest).addRequestHeader(anyString(), anyString())
    }

    private fun notifyDownloadFailed(id: String) {
        val intent = Intent(ACTION_DOWNLOAD_COMPLETE)
        intent.putExtra(android.app.DownloadManager.EXTRA_DOWNLOAD_ID, id)
        intent.putExtra(EXTRA_DOWNLOAD_STATUS, DownloadState.Status.FAILED)
        testContext.sendBroadcast(intent)
    }

    private fun notifyDownloadCompleted(id: String) {
        val intent = Intent(ACTION_DOWNLOAD_COMPLETE)
        intent.putExtra(android.app.DownloadManager.EXTRA_DOWNLOAD_ID, id)
        intent.putExtra(EXTRA_DOWNLOAD_STATUS, DownloadState.Status.COMPLETED)
        testContext.sendBroadcast(intent)
    }

    private fun grantPermissions() {
        grantPermission(INTERNET, WRITE_EXTERNAL_STORAGE)
    }
}

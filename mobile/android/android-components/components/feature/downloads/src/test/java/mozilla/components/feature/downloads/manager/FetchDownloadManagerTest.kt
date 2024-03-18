/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.manager

import android.Manifest.permission.FOREGROUND_SERVICE
import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.app.DownloadManager.ACTION_DOWNLOAD_COMPLETE
import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Looper.getMainLooper
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.EXTRA_DOWNLOAD_STATUS
import mozilla.components.support.base.android.NotificationsDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class FetchDownloadManagerTest {

    private lateinit var broadcastManager: LocalBroadcastManager
    private lateinit var service: MockDownloadService
    private lateinit var download: DownloadState
    private lateinit var downloadManager: FetchDownloadManager<MockDownloadService>
    private lateinit var store: BrowserStore
    private lateinit var notificationsDelegate: NotificationsDelegate

    @Before
    fun setup() {
        broadcastManager = LocalBroadcastManager.getInstance(testContext)
        service = MockDownloadService()
        store = BrowserStore()
        notificationsDelegate = mock()
        download = DownloadState(
            "http://ipv4.download.thinkbroadband.com/5MB.zip",
            "",
            "application/zip",
            5242880,
            userAgent = "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36",
        )
        downloadManager = FetchDownloadManager(
            testContext,
            store,
            MockDownloadService::class,
            notificationsDelegate = notificationsDelegate,
        )
    }

    @Test(expected = SecurityException::class)
    fun `calling download without the right permission must throw an exception`() {
        downloadManager.download(download)
    }

    @Test
    fun `calling download must queue the download`() {
        var downloadStopped = false

        downloadManager.onDownloadStopped = { _, _, _ -> downloadStopped = true }

        grantPermissions()

        assertTrue(store.state.downloads.isEmpty())
        val id = downloadManager.download(download)!!
        store.waitUntilIdle()
        assertEquals(download, store.state.downloads[download.id])

        notifyDownloadCompleted(id)
        shadowOf(getMainLooper()).idle()

        assertTrue(downloadStopped)
    }

    @Test
    fun `sending an ACTION_DOWNLOAD_COMPLETE intent without an EXTRA_DOWNLOAD_STATUS should not crash`() {
        var downloadStopped = false

        downloadManager.onDownloadStopped = { _, _, _ -> downloadStopped = true }

        grantPermissions()

        assertTrue(store.state.downloads.isEmpty())
        val id = downloadManager.download(download)!!
        store.waitUntilIdle()
        assertEquals(download, store.state.downloads[download.id])

        // Excluding the EXTRA_DOWNLOAD_STATUS
        val intent = Intent(ACTION_DOWNLOAD_COMPLETE)
        intent.putExtra(EXTRA_DOWNLOAD_ID, id)

        testContext.sendBroadcast(intent)

        shadowOf(getMainLooper()).idle()

        assertFalse(downloadStopped)
    }

    @Test
    fun `calling tryAgain starts the download again`() {
        val context = spy(testContext)
        downloadManager = FetchDownloadManager(
            context,
            store,
            MockDownloadService::class,
            notificationsDelegate = notificationsDelegate,
        )
        var downloadStopped = false

        downloadManager.onDownloadStopped = { _, _, _ -> downloadStopped = true }

        grantPermissions()

        val id = downloadManager.download(download)!!
        store.waitUntilIdle()
        notifyDownloadFailed(id)
        shadowOf(getMainLooper()).idle()
        assertTrue(downloadStopped)

        downloadStopped = false
        downloadManager.tryAgain(id)
        verify(context).startService(any())
        notifyDownloadCompleted(id)
        shadowOf(getMainLooper()).idle()

        assertTrue(downloadStopped)
    }

    @Test
    fun `GIVEN a device that supports scoped storage THEN permissions must not included file access`() {
        val downloadManagerSpy = spy(downloadManager)

        doReturn(Build.VERSION_CODES.Q).`when`(downloadManagerSpy).getSDKVersion()

        assertTrue(WRITE_EXTERNAL_STORAGE !in downloadManagerSpy.permissions)
    }

    @Test
    fun `GIVEN a device does not supports scoped storage THEN permissions must be included file access`() {
        val downloadManagerSpy = spy(downloadManager)

        doReturn(Build.VERSION_CODES.P).`when`(downloadManagerSpy).getSDKVersion()

        assertTrue(WRITE_EXTERNAL_STORAGE in downloadManagerSpy.permissions)

        doReturn(Build.VERSION_CODES.O_MR1).`when`(downloadManagerSpy).getSDKVersion()

        assertTrue(WRITE_EXTERNAL_STORAGE in downloadManagerSpy.permissions)
    }

    @Test
    fun `try again should not crash when download does not exist`() {
        val context: Context = mock()
        downloadManager = FetchDownloadManager(
            context,
            store,
            MockDownloadService::class,
            notificationsDelegate = notificationsDelegate,
        )
        grantPermissions()
        val id = downloadManager.download(download)!!

        downloadManager.tryAgain(id + 1)
        verify(context, never()).startService(any())
    }

    @Test
    fun `trying to download a file with invalid protocol must NOT triggered a download`() {
        val invalidDownload = download.copy(url = "ftp://ipv4.download.thinkbroadband.com/5MB.zip")
        grantPermissions()

        val id = downloadManager.download(invalidDownload)
        assertNull(id)
    }

    @Test
    fun `trying to download a file with a blob scheme should trigger a download`() {
        val validBlobDownload =
            download.copy(url = "blob:https://ipv4.download.thinkbroadband.com/5MB.zip")
        grantPermissions()

        val id = downloadManager.download(validBlobDownload)!!
        assertNotNull(id)
    }

    @Test
    fun `trying to download a file with a moz-extension scheme should trigger a download`() {
        val validBlobDownload =
            download.copy(url = "moz-extension://db84fb8b-909c-4270-8567-0e947ffe379f/readerview.html?id=1&url=https%3A%2F%2Fmozilla.org")
        grantPermissions()

        val id = downloadManager.download(validBlobDownload)!!
        assertNotNull(id)
    }

    @Test
    fun `sendBroadcast with valid downloadID must call onDownloadStopped after download`() {
        var downloadStopped = false
        var downloadStatus: DownloadState.Status? = null
        val downloadWithFileName = download.copy(fileName = "5MB.zip")

        grantPermissions()

        downloadManager.onDownloadStopped = { _, _, status ->
            downloadStatus = status
            downloadStopped = true
        }

        val id = downloadManager.download(
            downloadWithFileName,
            cookie = "yummy_cookie=choco",
        )!!
        store.waitUntilIdle()

        notifyDownloadCompleted(id)
        shadowOf(getMainLooper()).idle()

        assertTrue(downloadStopped)
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
            cookie = "yummy_cookie=choco",
        )!!
        store.waitUntilIdle()
        assertEquals(downloadWithFileName, store.state.downloads[downloadWithFileName.id])

        notifyDownloadCompleted(id)
        shadowOf(getMainLooper()).idle()

        store.waitUntilIdle()
        assertEquals(DownloadState.Status.COMPLETED, downloadStatus)
    }

    @Test
    fun `onReceive properly gets download object form sendBroadcast`() {
        var downloadStopped = false
        var downloadStatus: DownloadState.Status? = null
        var downloadName = ""
        var downloadSize = 0L
        val downloadWithFileName = download.copy(fileName = "5MB.zip", contentLength = 5L)

        grantPermissions()

        downloadManager.onDownloadStopped = { download, _, status ->
            downloadStatus = status
            downloadStopped = true
            downloadName = download.fileName ?: ""
            downloadSize = download.contentLength ?: 0
        }

        val id = downloadManager.download(downloadWithFileName)!!
        store.waitUntilIdle()
        notifyDownloadCompleted(id)
        shadowOf(getMainLooper()).idle()

        assertTrue(downloadStopped)
        assertEquals("5MB.zip", downloadName)
        assertEquals(5L, downloadSize)
        assertEquals(DownloadState.Status.COMPLETED, downloadStatus)
    }

    private fun notifyDownloadFailed(id: String) {
        val intent = Intent(ACTION_DOWNLOAD_COMPLETE)
        intent.putExtra(EXTRA_DOWNLOAD_ID, id)
        intent.putExtra(EXTRA_DOWNLOAD_STATUS, DownloadState.Status.FAILED)

        testContext.sendBroadcast(intent)
    }

    private fun notifyDownloadCompleted(id: String) {
        val intent = Intent(ACTION_DOWNLOAD_COMPLETE)
        intent.putExtra(EXTRA_DOWNLOAD_ID, id)
        intent.putExtra(EXTRA_DOWNLOAD_STATUS, DownloadState.Status.COMPLETED)

        testContext.sendBroadcast(intent)
    }

    private fun grantPermissions() {
        grantPermission(INTERNET, WRITE_EXTERNAL_STORAGE, FOREGROUND_SERVICE)
    }

    class MockDownloadService : AbstractFetchDownloadService() {
        override val httpClient: Client = mock()
        override val store: BrowserStore = mock()
        override val notificationsDelegate: NotificationsDelegate = mock()
    }
}

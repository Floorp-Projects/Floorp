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
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.concept.fetch.Client
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class FetchDownloadManagerTest {

    private lateinit var broadcastManager: LocalBroadcastManager
    private lateinit var service: MockDownloadService
    private lateinit var download: DownloadState
    private lateinit var downloadManager: FetchDownloadManager<MockDownloadService>

    @Before
    fun setup() {
        broadcastManager = LocalBroadcastManager.getInstance(testContext)
        service = MockDownloadService()
        download = DownloadState(
            "http://ipv4.download.thinkbroadband.com/5MB.zip",
            "", "application/zip", 5242880,
            "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36"
        )
        downloadManager = FetchDownloadManager(testContext, MockDownloadService::class, broadcastManager)
    }

    @Test(expected = SecurityException::class)
    fun `calling download without the right permission must throw an exception`() {
        downloadManager.download(download)
    }

    @Test
    fun `calling download must download the file`() {
        val context: Context = mock()
        downloadManager = FetchDownloadManager(context, MockDownloadService::class, broadcastManager)
        var downloadCompleted = false

        downloadManager.onDownloadCompleted = { _, _ -> downloadCompleted = true }

        grantPermissions()

        val id = downloadManager.download(download)!!

        verify(context).startService(any())

        notifyDownloadCompleted(id)

        assertTrue(downloadCompleted)
    }

    @Test
    fun `trying to download a file with invalid protocol must NOT triggered a download`() {

        val invalidDownload = download.copy(url = "ftp://ipv4.download.thinkbroadband.com/5MB.zip")

        grantPermissions()

        val id = downloadManager.download(invalidDownload)

        assertNull(id)
    }

    @Test
    fun `calling registerListener with valid downloadID must call listener after download`() {
        var downloadCompleted = false
        val downloadWithFileName = download.copy(fileName = "5MB.zip")

        grantPermissions()

        downloadManager.onDownloadCompleted = { _, _ -> downloadCompleted = true }

        val id = downloadManager.download(
            downloadWithFileName,
            cookie = "yummy_cookie=choco"
        )!!

        notifyDownloadCompleted(id)

        assertTrue(downloadCompleted)

        downloadCompleted = false
        notifyDownloadCompleted(id)

        assertFalse(downloadCompleted)
    }

    private fun notifyDownloadCompleted(id: Long) {
        val intent = Intent(ACTION_DOWNLOAD_COMPLETE)
        intent.putExtra(EXTRA_DOWNLOAD_ID, id)
        broadcastManager.sendBroadcast(intent)
    }

    private fun grantPermissions() {
        grantPermission(INTERNET, WRITE_EXTERNAL_STORAGE, FOREGROUND_SERVICE)
    }

    class MockDownloadService : AbstractFetchDownloadService() {
        override val httpClient: Client = mock()
    }
}

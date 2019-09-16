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
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.grantPermission
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions

@RunWith(AndroidJUnit4::class)
class AndroidDownloadManagerTest {

    private lateinit var download: DownloadState
    private lateinit var downloadManager: AndroidDownloadManager

    @Before
    fun setup() {
        download = DownloadState(
            "http://ipv4.download.thinkbroadband.com/5MB.zip",
            "", "application/zip", 5242880,
            "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36"
        )
        downloadManager = AndroidDownloadManager(testContext)
    }

    @Test(expected = SecurityException::class)
    fun `calling download without the right permission must throw an exception`() {
        downloadManager.download(download)
    }

    @Test
    fun `calling download must download the file`() {
        var downloadCompleted = false

        downloadManager.onDownloadCompleted = { _, _ -> downloadCompleted = true }

        grantPermissions()

        val id = downloadManager.download(download)!!

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

        val id = downloadManager.download(
            downloadWithFileName,
            cookie = "yummy_cookie=choco"
        )!!

        downloadManager.onDownloadCompleted = { _, _ -> downloadCompleted = true }

        notifyDownloadCompleted(id)

        assertTrue(downloadCompleted)

        downloadCompleted = false
        notifyDownloadCompleted(id)

        assertFalse(downloadCompleted)
    }

    @Test
    fun `no null or empty headers can be added to the DownloadManager`() {
        val mockRequest: Request = mock()

        mockRequest.addRequestHeaderSafely("User-Agent", "")

        verifyZeroInteractions(mockRequest)

        mockRequest.addRequestHeaderSafely("User-Agent", null)

        verifyZeroInteractions(mockRequest)

        val fireFox = "Mozilla/5.0 (Windows NT 5.1; rv:7.0.1) Gecko/20100101 Firefox/7.0.1"

        mockRequest.addRequestHeaderSafely("User-Agent", fireFox)

        verify(mockRequest).addRequestHeader(anyString(), anyString())
    }

    private fun notifyDownloadCompleted(id: Long) {
        val intent = Intent(ACTION_DOWNLOAD_COMPLETE)
        intent.putExtra(android.app.DownloadManager.EXTRA_DOWNLOAD_ID, id)
        testContext.sendBroadcast(intent)
    }

    private fun grantPermissions() {
        grantPermission(INTERNET, WRITE_EXTERNAL_STORAGE)
    }
}

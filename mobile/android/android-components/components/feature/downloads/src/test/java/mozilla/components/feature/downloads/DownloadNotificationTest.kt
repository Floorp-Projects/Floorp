/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService.DownloadJobState
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class DownloadNotificationTest {

    @Test
    fun getProgress() {
        val downloadJobState = DownloadJobState(
                job = null,
                state = DownloadState(url = "mozilla.org/mozilla.txt", contentLength = 100L,
                        currentBytesCopied = 10,
                        status = DownloadState.Status.DOWNLOADING),
                foregroundServiceId = 1,
                downloadDeleted = false,
                currentBytesCopied = 10,
                status = DownloadState.Status.DOWNLOADING
        )

        assertEquals("10%", downloadJobState.getProgress())

        val newDownload = downloadJobState.copy(state = downloadJobState.state.copy(contentLength = null))

        assertEquals("", newDownload.getProgress())
    }

    @Test
    fun setCompatGroup() {
        val notificationBuilder = NotificationCompat.Builder(testContext, "")
                .setCompatGroup("myGroup").build()

        assertEquals("myGroup", notificationBuilder.group)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `setCompatGroup will not set the group`() {
        val notificationBuilder = NotificationCompat.Builder(testContext, "")
                .setCompatGroup("myGroup").build()

        assertNotEquals("myGroup", notificationBuilder.group)
    }

    @Test
    fun getStatusDescription() {
        val pausedText = testContext.getString(R.string.mozac_feature_downloads_paused_notification_text)
        val completedText = testContext.getString(R.string.mozac_feature_downloads_completed_notification_text2)
        val failedText = testContext.getString(R.string.mozac_feature_downloads_failed_notification_text2)
        var downloadJobState = DownloadJobState(
                job = null,
                state = DownloadState(fileName = "mozilla.txt", url = "mozilla.org/mozilla.txt", contentLength = 100L,
                        currentBytesCopied = 10,
                        status = DownloadState.Status.DOWNLOADING),
                foregroundServiceId = 1,
                downloadDeleted = false,
                status = DownloadState.Status.DOWNLOADING,
                currentBytesCopied = 10
        )

        assertEquals(downloadJobState.getProgress(), downloadJobState.getStatusDescription(testContext))

        downloadJobState = DownloadJobState(
                job = null,
                state = DownloadState(fileName = "mozilla.txt", url = "mozilla.org/mozilla.txt", contentLength = 100L,
                        currentBytesCopied = 10,
                        status = DownloadState.Status.PAUSED),
                foregroundServiceId = 1,
                downloadDeleted = false,
                status = DownloadState.Status.PAUSED
        )

        assertEquals(pausedText, downloadJobState.getStatusDescription(testContext))

        downloadJobState = DownloadJobState(
                job = null,
                state = DownloadState(fileName = "mozilla.txt", url = "mozilla.org/mozilla.txt", contentLength = 100L,
                        currentBytesCopied = 10,
                        status = DownloadState.Status.COMPLETED),
                foregroundServiceId = 1,
                downloadDeleted = false,
                status = DownloadState.Status.COMPLETED
        )

        assertEquals(completedText, downloadJobState.getStatusDescription(testContext))

        downloadJobState = DownloadJobState(
                job = null,
                state = DownloadState(fileName = "mozilla.txt", url = "mozilla.org/mozilla.txt", contentLength = 100L,
                        currentBytesCopied = 10,
                        status = DownloadState.Status.FAILED),
                foregroundServiceId = 1,
                downloadDeleted = false,
                status = DownloadState.Status.FAILED
        )

        assertEquals(failedText, downloadJobState.getStatusDescription(testContext))

        downloadJobState = DownloadJobState(
                job = null,
                state = DownloadState(fileName = "mozilla.txt", url = "mozilla.org/mozilla.txt", contentLength = 100L,
                        currentBytesCopied = 10,
                        status = DownloadState.Status.CANCELLED),
                foregroundServiceId = 1,
                downloadDeleted = false,
                status = DownloadState.Status.CANCELLED
        )

        assertEquals("", downloadJobState.getStatusDescription(testContext))
    }

    @Test
    fun getDownloadSummary() {
        val download1 = DownloadJobState(
                job = null,
                state = DownloadState(fileName = "mozilla.txt", url = "mozilla.org/mozilla.txt", contentLength = 100L,
                        currentBytesCopied = 10,
                        status = DownloadState.Status.DOWNLOADING),
                foregroundServiceId = 1,
                downloadDeleted = false,
                currentBytesCopied = 10,
                status = DownloadState.Status.DOWNLOADING
        )
        val download2 = DownloadJobState(
                job = null,
                state = DownloadState(fileName = "mozilla2.txt", url = "mozilla.org/mozilla.txt", contentLength = 100L,
                        currentBytesCopied = 20,
                        status = DownloadState.Status.DOWNLOADING),
                foregroundServiceId = 1,
                downloadDeleted = false,
                currentBytesCopied = 20,
                status = DownloadState.Status.DOWNLOADING
        )

        val summary = DownloadNotification.getSummaryList(testContext, listOf(download1, download2))
        assertEquals(listOf("mozilla.txt 10%", "mozilla2.txt 20%"), summary)
    }
}
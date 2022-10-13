/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.db

import android.os.Environment
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DownloadEntityTest {

    @Test
    fun `convert a DownloadEntity to a DownloadState`() {
        val downloadEntity = DownloadEntity(
            id = "1",
            url = "url",
            fileName = "fileName",
            contentType = "application/zip",
            contentLength = 5242880,
            status = DownloadState.Status.DOWNLOADING,
            destinationDirectory = Environment.DIRECTORY_MUSIC,
            createdAt = 33,
        )

        val downloadState = downloadEntity.toDownloadState()

        assertEquals(downloadEntity.id, downloadState.id)
        assertEquals(downloadEntity.url, downloadState.url)
        assertEquals(downloadEntity.fileName, downloadState.fileName)
        assertEquals(downloadEntity.contentType, downloadState.contentType)
        assertEquals(downloadEntity.contentLength, downloadState.contentLength)
        assertEquals(downloadEntity.status, downloadState.status)
        assertEquals(downloadEntity.destinationDirectory, downloadState.destinationDirectory)
        assertEquals(downloadEntity.createdAt, downloadState.createdTime)
    }

    @Test
    fun `convert a DownloadState to DownloadEntity`() {
        val downloadState = DownloadState(
            id = "1",
            url = "url",
            fileName = "fileName",
            contentType = "application/zip",
            contentLength = 5242880,
            status = DownloadState.Status.DOWNLOADING,
            destinationDirectory = Environment.DIRECTORY_MUSIC,
            private = true,
            createdTime = 33,
        )

        val downloadEntity = downloadState.toDownloadEntity()

        assertEquals(downloadState.id, downloadEntity.id)
        assertEquals(downloadState.url, downloadEntity.url)
        assertEquals(downloadState.fileName, downloadEntity.fileName)
        assertEquals(downloadState.contentType, downloadEntity.contentType)
        assertEquals(downloadState.contentLength, downloadEntity.contentLength)
        assertEquals(downloadState.status, downloadEntity.status)
        assertEquals(downloadState.destinationDirectory, downloadEntity.destinationDirectory)
        assertEquals(downloadState.createdTime, downloadEntity.createdAt)
    }

    @Test
    fun `GIVEN a download with data URL WHEN converting a DownloadState to DownloadEntity THEN data url is removed`() {
        val downloadState = DownloadState(
            id = "1",
            url = "data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==",
        )

        val downloadEntity = downloadState.toDownloadEntity()

        assertEquals(downloadState.id, downloadEntity.id)
        assertTrue(downloadEntity.url.isEmpty())
    }

    @Test
    fun `GIVEN a download with no data URL WHEN converting a DownloadState to DownloadEntity THEN data url is not removed`() {
        val downloadState = DownloadState(
            id = "1",
            url = "url",
        )

        val downloadEntity = downloadState.toDownloadEntity()

        assertEquals(downloadState.id, downloadEntity.id)
        assertEquals(downloadState.url, downloadEntity.url)
    }
}

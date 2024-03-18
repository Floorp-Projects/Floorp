/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.os.Environment
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DownloadStorageTest {
    @Test
    fun isSameDownload() {
        val download = DownloadState(
            id = "1",
            url = "url",
            contentType = "application/zip",
            contentLength = 5242880,
            status = DownloadState.Status.DOWNLOADING,
            destinationDirectory = Environment.DIRECTORY_MUSIC,
        )

        assertTrue(DownloadStorage.isSameDownload(download, download))
        assertFalse(DownloadStorage.isSameDownload(download, download.copy(id = "2")))
        assertFalse(DownloadStorage.isSameDownload(download, download.copy(url = "newUrl")))
        assertFalse(DownloadStorage.isSameDownload(download, download.copy(contentType = "contentType")))
        assertFalse(DownloadStorage.isSameDownload(download, download.copy(contentLength = 0)))
        assertFalse(DownloadStorage.isSameDownload(download, download.copy(status = DownloadState.Status.COMPLETED)))
        assertFalse(DownloadStorage.isSameDownload(download, download.copy(destinationDirectory = Environment.DIRECTORY_DOWNLOADS)))
    }
}

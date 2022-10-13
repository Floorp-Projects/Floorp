/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.BYTES_TO_MB_LIMIT
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.KEY_FILE_NAME
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.KILOBYTE
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.MEGABYTE
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.roundToLong

@RunWith(AndroidJUnit4::class)
class DownloadDialogFragmentTest {

    private lateinit var dialog: DownloadDialogFragment
    private lateinit var download: DownloadState

    @Before
    fun setup() {
        dialog = object : DownloadDialogFragment() {}
        download = DownloadState(
            "http://ipv4.download.thinkbroadband.com/5MB.zip",
            "5MB.zip",
            "application/zip",
            5242880,
            userAgent = "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36",
        )
    }

    @Test
    fun `when setDownload must set download metadata`() {
        dialog.setDownload(download)

        assertNotNull(dialog.arguments)
        val fileName = dialog.arguments!![KEY_FILE_NAME]
        val url = dialog.arguments!![DownloadDialogFragment.KEY_URL]
        val contentLength = dialog.arguments!![DownloadDialogFragment.KEY_CONTENT_LENGTH]

        assertEquals(fileName, download.fileName)
        assertEquals(url, download.url)
        assertEquals(contentLength, download.contentLength)
    }

    @Test
    fun `extension function 'toMegabyteOrKilobyteString' returns MB string when size is or equal or bigger than the limit `() {
        val size = (BYTES_TO_MB_LIMIT * MEGABYTE).roundToLong()
        val expectedString = String.format("%.2f MB", size / MEGABYTE)
        assertEquals(expectedString, size.toMegabyteOrKilobyteString())
    }

    @Test
    fun `extension function 'toMegabyteOrKilobyteString' returns KB string when size is smaller than the limit `() {
        val size = (BYTES_TO_MB_LIMIT * MEGABYTE).roundToLong() - 1
        val expectedString = String.format("%.2f KB", size / KILOBYTE)
        assertEquals(expectedString, size.toMegabyteOrKilobyteString())
    }
}

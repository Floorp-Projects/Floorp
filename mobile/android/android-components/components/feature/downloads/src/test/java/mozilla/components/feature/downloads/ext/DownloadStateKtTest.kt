/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.ext

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.support.utils.DownloadUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DownloadStateKtTest {
    @Test
    fun `GIVEN a download filename is unkwnown WHEN requested with a guessing fallback THEN return a guessed canonical filename`() {
        val download = DownloadState(
            url = "url",
            fileName = null,
        )
        val expectedName = with(download) {
            DownloadUtils.guessFileName(null, destinationDirectory, url, contentType)
        }

        val result = download.realFilenameOrGuessed

        assertEquals(expectedName, result)
    }

    @Test
    fun `GIVEN a download filename is available WHEN requested with a guessing fallback THEN return the available filename`() {
        val download = DownloadState(
            url = "http://example.com/file.jpg",
            fileName = "test",
            contentType = "image/jpeg",
        )
        val guessedName = with(download) {
            DownloadUtils.guessFileName(null, destinationDirectory, url, contentType)
        }

        val result = download.realFilenameOrGuessed

        assertEquals("test", result)
        assertNotEquals(guessedName, result)
    }
}

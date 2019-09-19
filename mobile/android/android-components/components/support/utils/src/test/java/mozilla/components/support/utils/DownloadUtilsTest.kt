/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.webkit.MimeTypeMap
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Shadows

@RunWith(AndroidJUnit4::class)
class DownloadUtilsTest {

    private fun assertContentDisposition(expected: String, contentDisposition: String) {
        assertEquals(expected, DownloadUtils.guessFileName(contentDisposition, null, null))
    }

    @Test
    fun guessFileName_contentDisposition() {
        // Default file name
        assertContentDisposition("downloadfile.bin", "")

        CONTENT_DISPOSITION_TYPES.forEach { contentDisposition ->
            // continuing with default filenames
            assertContentDisposition("downloadfile.bin", contentDisposition)
            assertContentDisposition("downloadfile.bin", "$contentDisposition;")
            assertContentDisposition("downloadfile.bin", "$contentDisposition; filename")
            assertContentDisposition(".bin", "$contentDisposition; filename=")
            assertContentDisposition(".bin", "$contentDisposition; filename=\"\"")

            // Provided filename field
            assertContentDisposition("filename.jpg", "$contentDisposition; filename=\"filename.jpg\"")
            assertContentDisposition("file\"name.jpg", "$contentDisposition; filename=\"file\\\"name.jpg\"")
            assertContentDisposition("file\\name.jpg", "$contentDisposition; filename=\"file\\\\name.jpg\"")
            assertContentDisposition("file\\\"name.jpg", "$contentDisposition; filename=\"file\\\\\\\"name.jpg\"")
            assertContentDisposition("filename.jpg", "$contentDisposition; filename=filename.jpg")
            assertContentDisposition("filename.jpg", "$contentDisposition; filename=filename.jpg; foo")
            assertContentDisposition("filename.jpg", "$contentDisposition; filename=\"filename.jpg\"; foo")

            // UTF-8 encoded filename* field
            assertContentDisposition("\uD83E\uDD8A + x.jpg",
                "$contentDisposition; filename=\"_.jpg\"; filename*=utf-8'en'%F0%9F%A6%8A%20+%20x.jpg")
            assertContentDisposition("filename 的副本.jpg",
                    contentDisposition + ";filename=\"_.jpg\";" +
                            "filename*=UTF-8''filename%20%E7%9A%84%E5%89%AF%E6%9C%AC.jpg")
            assertContentDisposition("filename.jpg",
                "$contentDisposition; filename=_.jpg; filename*=utf-8'en'filename.jpg")

            // ISO-8859-1 encoded filename* field
            assertContentDisposition("file' 'name.jpg",
                "$contentDisposition; filename=\"_.jpg\"; filename*=iso-8859-1'en'file%27%20%27name.jpg")
        }
    }

    @Test
    fun guessFileName_url() {
        assertUrl("downloadfile.bin", "http://example.com/")
        assertUrl("downloadfile.bin", "http://example.com/filename/")
        assertUrl("filename.jpg", "http://example.com/filename.jpg")
        assertUrl("filename.jpg", "http://example.com/foo/bar/filename.jpg")
    }

    @Test
    fun guessFileName_mimeType() {
        Shadows.shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypMapping("jpg", "image/jpeg")
        Shadows.shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypMapping("zip", "application/zip")
        Shadows.shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypMapping("tar.gz", "application/gzip")

        assertEquals("file.jpg", DownloadUtils.guessFileName(null, "http://example.com/file.jpg", "image/jpeg"))
        assertEquals("file.jpg", DownloadUtils.guessFileName(null, "http://example.com/file.bin", "image/jpeg"))
        assertEquals(
            "Caesium-wahoo-v3.6-b792615ced1b.zip",
            DownloadUtils.guessFileName(null, "https://download.msfjarvis.website/caesium/wahoo/beta/Caesium-wahoo-v3.6-b792615ced1b.zip", "application/zip")
        )
        assertEquals(
            "compressed.TAR.GZ",
            DownloadUtils.guessFileName(null, "http://example.com/compressed.TAR.GZ", "application/gzip")
        )
    }

    companion object {
        private val CONTENT_DISPOSITION_TYPES = listOf("attachment", "inline")

        private fun assertUrl(expected: String, url: String) {
            assertEquals(expected, DownloadUtils.guessFileName(null, url, null))
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.util.AtomicFile
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.manifest.WebAppManifestParser
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.FileOutputStream

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class ManifestStorageTest {
    @Test
    fun `load returns null if entry does not exist or is invalid`() = runBlocking {
        val storage = spy(ManifestStorage(testContext))
        assertNull(storage.loadManifest("https://example.com"))

        val mozFile = storage.mockFile("https://mozilla.org")
        val fireFile = storage.mockFile("https://firefox.com")
        whenever(mozFile.readFully()).thenReturn("[]".toByteArray())
        whenever(fireFile.readFully()).thenReturn("{}".toByteArray())

        assertNull(storage.loadManifest("https://mozilla.org"))
        assertNull(storage.loadManifest("https://firefox.com"))
    }

    @Test
    fun `load returns valid manifest`() = runBlocking {
        val storage = spy(ManifestStorage(testContext))
        val file = storage.mockFile("https://mozilla.org")
        whenever(file.readFully())
            .thenReturn("{\"name\":\"Mozilla\",\"start_url\":\"https://mozilla.org\"}".toByteArray())

        val expected = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        assertEquals(expected, storage.loadManifest("https://mozilla.org"))
    }

    @Test
    fun `save saves the manifest as JSON`() = runBlocking {
        val storage = spy(ManifestStorage(testContext))
        val manifest = WebAppManifest(name = "Firefox", startUrl = "https://firefox.com")
        val json = WebAppManifestParser().serialize(manifest)

        val stream = mock<FileOutputStream>()
        val file = storage.mockFile("https://firefox.com")
        whenever(file.startWrite()).thenReturn(stream)

        storage.saveManifest(manifest)
        verify(stream).write(json.toString().toByteArray())
        Unit
    }

    @Test
    fun `remove deletes a saved manifest`() = runBlocking {
        val storage = spy(ManifestStorage(testContext))
        val file = storage.mockFile("https://example.com")

        storage.removeManifests(listOf("https://example.com"))
        verify(file).delete()
        Unit
    }

    @Test
    fun `remove deletes many manifests`() = runBlocking {
        val storage = spy(ManifestStorage(testContext))
        val exampleFile = storage.mockFile("https://example.com")
        val proxxFile = storage.mockFile("https://proxx.app")

        storage.removeManifests(listOf("https://example.com", "https://proxx.app"))
        verify(exampleFile).delete()
        verify(proxxFile).delete()
        Unit
    }

    private fun ManifestStorage.mockFile(startUrl: String) =
        mock<AtomicFile>().also {
            doReturn(it).`when`(this).getFileForManifest(startUrl)
        }
}

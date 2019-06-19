/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.db.ManifestDao
import mozilla.components.feature.pwa.db.ManifestEntity
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class ManifestStorageTest {
    @Test
    fun `load returns null if entry does not exist`() = runBlocking {
        val storage = spy(ManifestStorage(testContext))
        mockDatabase(storage)
        assertNull(storage.loadManifest("https://example.com"))
    }

    @Test
    fun `load returns valid manifest`() = runBlocking {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)

        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        whenever(dao.getManifest("https://mozilla.org"))
            .thenReturn(ManifestEntity(manifest))

        assertEquals(manifest, storage.loadManifest("https://mozilla.org"))
    }

    @Test
    fun `save saves the manifest as JSON`() = runBlocking {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)
        val manifest = WebAppManifest(name = "Firefox", startUrl = "https://firefox.com")

        storage.saveManifest(manifest)
        verify(dao).insertManifest(any())
        Unit
    }

    @Test
    fun `remove deletes saved manifests`() = runBlocking {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)

        storage.removeManifests(listOf("https://example.com", "https://proxx.app"))
        verify(dao).deleteManifests(listOf("https://example.com", "https://proxx.app"))
        Unit
    }

    private fun mockDatabase(storage: ManifestStorage): ManifestDao = mock<ManifestDao>().also {
        storage.manifestDao = lazy { it }
    }
}

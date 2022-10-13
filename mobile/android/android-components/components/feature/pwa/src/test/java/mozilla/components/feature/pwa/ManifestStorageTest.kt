/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.db.ManifestDao
import mozilla.components.feature.pwa.db.ManifestEntity
import mozilla.components.support.test.any
import mozilla.components.support.test.capture
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentCaptor
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class ManifestStorageTest {

    private val firefoxManifest = WebAppManifest(
        name = "Firefox",
        startUrl = "https://firefox.com",
        scope = "/",
    )

    private val googleMapsManifest = WebAppManifest(
        name = "Google Maps",
        startUrl = "https://google.com/maps",
        scope = "https://google.com/maps/",
    )

    private val exampleWebAppManifest = WebAppManifest(
        name = "Example Web App",
        startUrl = "https://pwa.example.com/dashboard",
        scope = "https://pwa.example.com/",
    )

    @Test
    fun `load returns null if entry does not exist`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        mockDatabase(storage)
        assertNull(storage.loadManifest("https://example.com"))
    }

    @Test
    fun `load returns valid manifest`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)

        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        whenever(dao.getManifest("https://mozilla.org"))
            .thenReturn(ManifestEntity(manifest))

        assertEquals(manifest, storage.loadManifest("https://mozilla.org"))
    }

    @Test
    fun `save saves the manifest as JSON`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)

        storage.saveManifest(firefoxManifest)
        verify(dao).insertManifest(any())
        Unit
    }

    @Test
    fun `update replaces the manifest as JSON`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)
        val existing = ManifestEntity(firefoxManifest, currentTime = 0)

        `when`(dao.getManifest("https://firefox.com")).thenReturn(existing)

        storage.updateManifest(firefoxManifest)
        verify(dao).updateManifest(any())
        Unit
    }

    @Test
    fun `update does not replace non-existed manifest`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)

        `when`(dao.getManifest("https://firefox.com")).thenReturn(null)

        storage.updateManifest(firefoxManifest)
        verify(dao, never()).updateManifest(any())
        Unit
    }

    @Test
    fun `remove deletes saved manifests`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)

        storage.removeManifests(listOf("https://example.com", "https://proxx.app"))
        verify(dao).deleteManifests(listOf("https://example.com", "https://proxx.app"))
        Unit
    }

    @Test
    fun `loading manifests by scope returns list of manifests`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)
        val manifest1 = WebAppManifest(name = "Mozilla1", startUrl = "https://mozilla.org", scope = "https://mozilla.org/pwa/1/")
        val manifest2 = WebAppManifest(name = "Mozilla2", startUrl = "https://mozilla.org", scope = "https://mozilla.org/pwa/1/")
        val manifest3 = WebAppManifest(name = "Mozilla3", startUrl = "https://mozilla.org", scope = "https://mozilla.org/pwa/")

        whenever(dao.getManifestsByScope("https://mozilla.org/index.html?key=value"))
            .thenReturn(listOf(ManifestEntity(manifest1), ManifestEntity(manifest2), ManifestEntity(manifest3)))

        assertEquals(
            listOf(manifest1, manifest2, manifest3),
            storage.loadManifestsByScope("https://mozilla.org/index.html?key=value"),
        )
    }

    @Test
    fun `loading manifests with share targets returns list of manifests`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)
        val manifest1 = WebAppManifest(
            name = "Mozilla",
            startUrl = "https://mozilla.org",
            shareTarget = WebAppManifest.ShareTarget("https://mozilla.org/share"),
        )
        val manifest2 = WebAppManifest(
            name = "Firefox",
            startUrl = "https://firefox.com",
            shareTarget = WebAppManifest.ShareTarget("https://firefox.com/share"),
        )
        val timeout = ManifestStorage.ACTIVE_THRESHOLD_MS
        val currentTime = System.currentTimeMillis()
        val deadline = currentTime - timeout

        whenever(dao.getRecentShareableManifests(deadline))
            .thenReturn(listOf(ManifestEntity(manifest1), ManifestEntity(manifest2)))

        assertEquals(
            listOf(manifest1, manifest2),
            storage.loadShareableManifests(currentTime),
        )
    }

    @Test
    fun `updateManifestUsedAt updates usedAt to current timestamp`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)
        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        val entity = ManifestEntity(manifest, currentTime = 0)

        val entityCaptor = ArgumentCaptor.forClass(ManifestEntity::class.java)

        whenever(dao.getManifest(manifest.startUrl))
            .thenReturn(entity)

        assertEquals(0, entity.usedAt)

        storage.updateManifestUsedAt(manifest)

        verify(dao).updateManifest(capture<ManifestEntity>(entityCaptor))
        assert(entityCaptor.value.usedAt > 0)
    }

    @Test
    fun `has recent manifest returns false if no manifest is found`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)
        val timeout = ManifestStorage.ACTIVE_THRESHOLD_MS
        val currentTime = System.currentTimeMillis()
        val deadline = currentTime - timeout

        whenever(dao.hasRecentManifest("https://mozilla.org/", deadline))
            .thenReturn(0)

        assertFalse(storage.hasRecentManifest("https://mozilla.org/", currentTime))
    }

    @Test
    fun `has recent manifest returns true if one or more manifests have been found`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)
        val timeout = ManifestStorage.ACTIVE_THRESHOLD_MS
        val currentTime = System.currentTimeMillis()
        val deadline = currentTime - timeout

        whenever(dao.hasRecentManifest("https://mozilla.org/", deadline))
            .thenReturn(1)

        assertTrue(storage.hasRecentManifest("https://mozilla.org/", currentTime))

        whenever(dao.hasRecentManifest("https://mozilla.org/", deadline))
            .thenReturn(5)

        assertTrue(storage.hasRecentManifest("https://mozilla.org/", currentTime))
    }

    @Test
    fun `recently used manifest count`() = runTest {
        val testThreshold = 1000 * 60 * 24L
        val storage = spy(ManifestStorage(testContext, activeThresholdMs = testThreshold))
        val dao = mockDatabase(storage)
        val currentTime = System.currentTimeMillis()
        val deadline = currentTime - testThreshold

        whenever(dao.recentManifestsCount(deadline))
            .thenReturn(0)

        assertEquals(0, storage.recentManifestsCount(currentTimeMs = currentTime))

        whenever(dao.recentManifestsCount(deadline))
            .thenReturn(5)

        assertEquals(5, storage.recentManifestsCount(currentTimeMs = currentTime))

        whenever(dao.recentManifestsCount(deadline - 10L))
            .thenReturn(3)

        assertEquals(
            3,
            storage.recentManifestsCount(
                activeThresholdMs = testThreshold + 10L,
                currentTimeMs = currentTime,
            ),
        )
    }

    @Test
    fun `warmUpScopes populates cache of already installed web app scopes`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)

        val manifest1 = ManifestEntity(manifest = firefoxManifest, currentTime = 0)
        val manifest2 = ManifestEntity(manifest = googleMapsManifest, currentTime = 0)
        val manifest3 = ManifestEntity(manifest = exampleWebAppManifest, currentTime = 0)

        whenever(dao.getInstalledScopes(0)).thenReturn(listOf(manifest1, manifest2, manifest3))

        storage.warmUpScopes(ManifestStorage.ACTIVE_THRESHOLD_MS)

        assertEquals(
            mapOf(
                Pair("/", "https://firefox.com"),
                Pair("https://google.com/maps/", "https://google.com/maps"),
                Pair("https://pwa.example.com/", "https://pwa.example.com/dashboard"),
            ),
            storage.installedScopes,
        )
    }

    @Test
    fun `getInstalledScope returns cached scope for an url`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)

        val manifest1 = ManifestEntity(manifest = firefoxManifest, currentTime = 0)
        val manifest2 = ManifestEntity(manifest = googleMapsManifest, currentTime = 0)
        val manifest3 = ManifestEntity(manifest = exampleWebAppManifest, currentTime = 0)

        whenever(dao.getInstalledScopes(0)).thenReturn(listOf(manifest1, manifest2, manifest3))

        storage.warmUpScopes(ManifestStorage.ACTIVE_THRESHOLD_MS)

        val result = storage.getInstalledScope("https://pwa.example.com/profile/me")

        assertEquals("https://pwa.example.com/", result)
    }

    @Test
    fun `getStartUrlForInstalledScope returns cached start url for a currently installed scope`() = runTest {
        val storage = spy(ManifestStorage(testContext))
        val dao = mockDatabase(storage)

        val manifest1 = ManifestEntity(manifest = firefoxManifest, currentTime = 0)
        val manifest2 = ManifestEntity(manifest = googleMapsManifest, currentTime = 0)
        val manifest3 = ManifestEntity(manifest = exampleWebAppManifest, currentTime = 0)

        whenever(dao.getInstalledScopes(0)).thenReturn(listOf(manifest1, manifest2, manifest3))

        storage.warmUpScopes(ManifestStorage.ACTIVE_THRESHOLD_MS)

        val result = storage.getStartUrlForInstalledScope("https://pwa.example.com/")

        assertEquals("https://pwa.example.com/dashboard", result)
    }

    private fun mockDatabase(storage: ManifestStorage): ManifestDao = mock<ManifestDao>().also {
        storage.manifestDao = lazy { it }
    }
}

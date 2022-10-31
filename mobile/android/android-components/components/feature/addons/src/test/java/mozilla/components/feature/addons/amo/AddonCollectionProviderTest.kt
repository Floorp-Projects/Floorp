/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.addons.Addon
import mozilla.components.support.test.any
import mozilla.components.support.test.file.loadResourceAsString
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.Date
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class AddonCollectionProviderTest {

    @Test
    fun `getAvailableAddons - with a successful status response must contain add-ons`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/collection.json"))
        val provider = AddonCollectionProvider(testContext, client = mockedClient)
        val addons = provider.getAvailableAddons()
        val addon = addons.first()

        assertTrue(addons.isNotEmpty())

        // Add-on details
        assertEquals("uBlock0@raymondhill.net", addon.id)
        assertEquals("2015-04-25T07:26:22Z", addon.createdAt)
        assertEquals("2019-10-24T09:28:41Z", addon.updatedAt)
        assertEquals(
            "https://addons.cdn.mozilla.net/user-media/addon_icons/607/607454-64.png?modified=mcrushed",
            addon.iconUrl,
        )
        assertEquals(
            "https://addons.mozilla.org/en-US/firefox/addon/ublock-origin/",
            addon.siteUrl,
        )
        assertEquals(
            "3428595",
            addon.downloadId,
        )
        assertEquals(
            "https://addons.mozilla.org/firefox/downloads/file/3428595/ublock_origin-1.23.0-an+fx.xpi?src=",
            addon.downloadUrl,
        )
        assertEquals(
            "menus",
            addon.permissions.first(),
        )
        assertEquals(
            "uBlock Origin",
            addon.translatableName["ca"],
        )
        assertEquals(
            "Finalment, un blocador eficient que utilitza pocs recursos de memòria i processador.",
            addon.translatableSummary["ca"],
        )
        assertTrue(addon.translatableDescription.getValue("ca").isNotBlank())
        assertEquals("1.23.0", addon.version)
        assertEquals("es", addon.defaultLocale)

        // Authors
        assertEquals("11423598", addon.authors.first().id)
        assertEquals("Raymond Hill", addon.authors.first().name)
        assertEquals("gorhill", addon.authors.first().username)
        assertEquals(
            "https://addons.mozilla.org/en-US/firefox/user/11423598/",
            addon.authors.first().url,
        )

        // Ratings
        assertEquals(4.7003F, addon.rating!!.average, 0.7003F)
        assertEquals(9930, addon.rating!!.reviews)
    }

    @Test
    fun `getAvailableAddons - with a successful status response must handle empty values`() = runTest {
        val client = prepareClient()
        val provider = AddonCollectionProvider(testContext, client = client)

        val addons = provider.getAvailableAddons()
        val addon = addons.first()

        assertTrue(addons.isNotEmpty())

        // Add-on
        assertEquals("", addon.id)
        assertEquals("", addon.createdAt)
        assertEquals("", addon.updatedAt)
        assertEquals("", addon.iconUrl)
        assertEquals("", addon.siteUrl)
        assertEquals("", addon.version)
        assertEquals("", addon.downloadId)
        assertEquals("", addon.downloadUrl)
        assertTrue(addon.permissions.isEmpty())
        assertTrue(addon.translatableName.isEmpty())
        assertTrue(addon.translatableSummary.isEmpty())
        assertEquals("", addon.translatableDescription.getValue("ca"))
        assertEquals(Addon.DEFAULT_LOCALE, addon.defaultLocale)

        // Authors
        assertTrue(addon.authors.isEmpty())
        verify(client).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "7e8d6dc651b54ab385fb8791bf9dac/addons/?page_size=$PAGE_SIZE&sort=${SortOption.POPULARITY_DESC.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        // Ratings
        assertNull(addon.rating)
    }

    @Test
    fun `getAvailableAddons - with a language`() = runTest {
        val client = prepareClient(loadResourceAsString("/localized_collection.json"))
        val provider = AddonCollectionProvider(testContext, client = client)

        val addons = provider.getAvailableAddons(language = "en")
        val addon = addons.first()

        assertTrue(addons.isNotEmpty())

        // Add-on
        assertEquals("uBlock0@raymondhill.net", addon.id)
        assertEquals("2015-04-25T07:26:22Z", addon.createdAt)
        assertEquals("2021-02-04T12:05:14Z", addon.updatedAt)
        assertEquals(
            "https://addons.cdn.mozilla.net/user-media/addon_icons/607/607454-64.png?modified=mcrushed",
            addon.iconUrl,
        )
        assertEquals(
            "https://addons.mozilla.org/en-US/firefox/addon/ublock-origin/",
            addon.siteUrl,
        )
        assertEquals(
            "3719054",
            addon.downloadId,
        )
        assertEquals(
            "https://addons.mozilla.org/firefox/downloads/file/3719054/ublock_origin-1.33.2-an+fx.xpi",
            addon.downloadUrl,
        )
        assertEquals(
            "dns",
            addon.permissions.first(),
        )
        assertEquals(
            "uBlock Origin",
            addon.translatableName["en"],
        )

        assertEquals(
            "Finally, an efficient wide-spectrum content blocker. Easy on CPU and memory.",
            addon.translatableSummary["en"],
        )

        assertTrue(addon.translatableDescription.getValue("en").isNotBlank())
        assertTrue(addon.translatableDescription.getValue("en").isNotBlank())
        assertEquals("1.33.2", addon.version)
        assertEquals("en", addon.defaultLocale)

        // Authors
        assertEquals("11423598", addon.authors.first().id)
        assertEquals("Raymond Hill", addon.authors.first().name)
        assertEquals("gorhill", addon.authors.first().username)
        assertEquals(
            "https://addons.mozilla.org/en-US/firefox/user/11423598/",
            addon.authors.first().url,
        )

        // Ratings
        assertEquals(4.7003F, addon.rating!!.average, 0.7003F)
        assertEquals(13324, addon.rating!!.reviews)

        verify(client).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "7e8d6dc651b54ab385fb8791bf9dac/addons/?page_size=$PAGE_SIZE&sort=${SortOption.POPULARITY_DESC.value}&lang=en",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        Unit
    }

    @Test
    fun `getAvailableAddons - read timeout can be configured`() = runTest {
        val mockedClient = prepareClient()

        val provider = spy(AddonCollectionProvider(testContext, client = mockedClient))
        provider.getAvailableAddons(readTimeoutInSeconds = 5)
        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "7e8d6dc651b54ab385fb8791bf9dac/addons/?page_size=$PAGE_SIZE&sort=${SortOption.POPULARITY_DESC.value}",
                readTimeout = Pair(5, TimeUnit.SECONDS),
            ),
        )
        Unit
    }

    @Test(expected = IOException::class)
    fun `getAvailableAddons - with unexpected status will throw exception`() = runTest {
        val mockedClient = prepareClient(status = 500)
        val provider = AddonCollectionProvider(testContext, client = mockedClient)
        provider.getAvailableAddons()
        Unit
    }

    @Test
    fun `getAvailableAddons - returns cached result if allowed and not expired`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/collection.json"))

        val provider = spy(AddonCollectionProvider(testContext, client = mockedClient))
        provider.getAvailableAddons(false)
        verify(provider, never()).readFromDiskCache(null, useFallbackFile = false)

        whenever(provider.cacheExpired(testContext, null, useFallbackFile = false)).thenReturn(true)
        provider.getAvailableAddons(true)
        verify(provider, never()).readFromDiskCache(null, useFallbackFile = false)

        whenever(provider.cacheExpired(testContext, null, useFallbackFile = false)).thenReturn(false)
        provider.getAvailableAddons(true)
        verify(provider).readFromDiskCache(null, useFallbackFile = false)
        Unit
    }

    @Test
    fun `getAvailableAddons - returns cached result if allowed and fetch failed`() = runTest {
        val mockedClient: Client = mock()
        val exception = IOException("test")
        val cachedAddons: List<Addon> = emptyList()
        whenever(mockedClient.fetch(any())).thenThrow(exception)

        val provider = spy(AddonCollectionProvider(testContext, client = mockedClient))

        try {
            // allowCache = false
            provider.getAvailableAddons(allowCache = false)
            fail("Expected IOException")
        } catch (e: IOException) {
            assertSame(exception, e)
        }

        try {
            // allowCache = true, but no cache present
            provider.getAvailableAddons(allowCache = true)
            fail("Expected IOException")
        } catch (e: IOException) {
            assertSame(exception, e)
        }

        try {
            // allowCache = true, cache present, but we fail to read
            whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(Date().time)
            provider.getAvailableAddons(allowCache = true)
            fail("Expected IOException")
        } catch (e: IOException) {
            assertSame(exception, e)
        }

//        // allowCache = true, cache present for a fallback file, and reading successfully
        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = true)).thenReturn(Date().time)
        whenever(provider.readFromDiskCache(null, useFallbackFile = true)).thenReturn(cachedAddons)
        assertSame(cachedAddons, provider.getAvailableAddons(allowCache = true))

        // allowCache = true, cache present, and reading successfully
        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(Date().time)
        whenever(provider.cacheExpired(testContext, null, useFallbackFile = false)).thenReturn(false)
        whenever(provider.readFromDiskCache(null, useFallbackFile = false)).thenReturn(cachedAddons)
        assertSame(cachedAddons, provider.getAvailableAddons(allowCache = true))
    }

    @Test
    fun `getAvailableAddons - writes response to cache if configured`() = runTest {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = prepareClient(jsonResponse)

        val provider = spy(AddonCollectionProvider(testContext, client = mockedClient))
        val cachingProvider = spy(AddonCollectionProvider(testContext, client = mockedClient, maxCacheAgeInMinutes = 1))

        provider.getAvailableAddons()
        verify(provider, never()).writeToDiskCache(jsonResponse, null)

        cachingProvider.getAvailableAddons()
        verify(cachingProvider).writeToDiskCache(jsonResponse, null)
    }

    @Test
    fun `getAvailableAddons - deletes unused cache files`() = runTest {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = prepareClient(jsonResponse)

        val provider = spy(AddonCollectionProvider(testContext, client = mockedClient, maxCacheAgeInMinutes = 1))

        provider.getAvailableAddons()
        verify(provider).deleteUnusedCacheFiles(null)
    }

    @Test
    fun `deleteUnusedCacheFiles - only deletes collection cache files`() {
        val regularFile = File(testContext.filesDir, "test.json")
        regularFile.createNewFile()
        assertTrue(regularFile.exists())

        val regularDir = File(testContext.filesDir, "testDir")
        regularDir.mkdir()
        assertTrue(regularDir.exists())

        val collectionFile = File(testContext.filesDir, COLLECTION_FILE_NAME.format("testCollection"))
        collectionFile.createNewFile()
        assertTrue(collectionFile.exists())

        val provider = AddonCollectionProvider(testContext, client = prepareClient(), maxCacheAgeInMinutes = 1)
        provider.deleteUnusedCacheFiles(null)
        assertTrue(regularFile.exists())
        assertTrue(regularDir.exists())
        assertFalse(collectionFile.exists())
    }

    @Test
    fun `deleteUnusedCacheFiles - will not remove the fallback localized file`() {
        val regularFile = File(testContext.filesDir, "test.json")
        regularFile.createNewFile()
        assertTrue(regularFile.exists())

        val regularDir = File(testContext.filesDir, "testDir")
        regularDir.mkdir()
        assertTrue(regularDir.exists())

        val provider = AddonCollectionProvider(testContext, client = prepareClient(), maxCacheAgeInMinutes = 1)
        val enFile = File(testContext.filesDir, provider.getCacheFileName("en"))

        enFile.createNewFile()

        assertTrue(enFile.exists())

        provider.deleteUnusedCacheFiles("es")

        val file = provider.getBaseCacheFile(testContext, "es", useFallbackFile = true)

        assertTrue(file.name.contains("en"))
        assertTrue(file.exists())
        assertEquals(enFile.name, file.name)
        assertTrue(regularFile.exists())
        assertTrue(regularDir.exists())
        assertTrue(enFile.delete())
        assertFalse(file.exists())
        assertTrue(regularFile.delete())
        assertTrue(regularDir.delete())
    }

    @Test
    fun `getBaseCacheFile - will return a first localized file WHEN the provided language file is not available`() {
        val provider = AddonCollectionProvider(testContext, client = prepareClient(), maxCacheAgeInMinutes = 1)
        val enFile = File(testContext.filesDir, provider.getCacheFileName("en"))

        enFile.createNewFile()

        assertTrue(enFile.exists())

        val file = provider.getBaseCacheFile(testContext, "es", useFallbackFile = true)

        assertTrue(file.name.contains("en"))
        assertTrue(file.exists())
        assertEquals(enFile.name, file.name)

        assertTrue(enFile.delete())
        assertFalse(file.exists())
    }

    @Test
    fun `getAvailableAddons - cache expiration check`() {
        var provider = spy(AddonCollectionProvider(testContext, client = mock(), maxCacheAgeInMinutes = -1))
        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(Date().time)
        assertTrue(provider.cacheExpired(testContext, null, useFallbackFile = false))

        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(-1)
        assertTrue(provider.cacheExpired(testContext, null, useFallbackFile = false))

        provider = spy(AddonCollectionProvider(testContext, client = mock(), maxCacheAgeInMinutes = 10))
        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(-1)
        assertTrue(provider.cacheExpired(testContext, null, useFallbackFile = false))

        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(Date().time - 60 * MINUTE_IN_MS)
        assertTrue(provider.cacheExpired(testContext, null, useFallbackFile = false))

        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(Date().time + 60 * MINUTE_IN_MS)
        assertFalse(provider.cacheExpired(testContext, null, useFallbackFile = false))
    }

    @Test
    fun `getAddonIconBitmap - with a successful status will return a bitmap`() = runTest {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val stream: InputStream = javaClass.getResourceAsStream("/png/mozac.png")!!.buffered()
        val responseBody = Response.Body(stream)

        whenever(mockedResponse.body).thenReturn(responseBody)
        whenever(mockedResponse.status).thenReturn(200)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)

        val provider = AddonCollectionProvider(testContext, client = mockedClient)
        val addon = Addon(
            id = "id",
            authors = mock(),
            categories = mock(),
            downloadUrl = "https://example.com",
            version = "version",
            iconUrl = "https://example.com/image.png",
            createdAt = "0",
            updatedAt = "0",
        )

        val bitmap = provider.getAddonIconBitmap(addon)
        assertTrue(bitmap is Bitmap)
    }

    @Test
    fun `getAddonIconBitmap - with an unsuccessful status will return null`() = runTest {
        val mockedClient = prepareClient(status = 500)
        val provider = AddonCollectionProvider(testContext, client = mockedClient)
        val addon = Addon(
            id = "id",
            authors = mock(),
            categories = mock(),
            downloadUrl = "https://example.com",
            version = "version",
            iconUrl = "https://example.com/image.png",
            createdAt = "0",
            updatedAt = "0",
        )

        val bitmap = provider.getAddonIconBitmap(addon)
        assertNull(bitmap)
    }

    @Test
    fun `collection name can be configured`() = runTest {
        val mockedClient = prepareClient()

        val collectionName = "collection123"
        val provider = AddonCollectionProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
        )

        provider.getAvailableAddons()
        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.POPULARITY_DESC.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        assertEquals(COLLECTION_FILE_NAME.format(collectionName), provider.getCacheFileName())
    }

    @Test
    fun `collection sort option can be specified`() = runTest {
        val mockedClient = prepareClient()

        val collectionName = "collection123"
        AddonCollectionProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.POPULARITY,
        ).also {
            it.getAvailableAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.POPULARITY.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AddonCollectionProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.POPULARITY_DESC,
        ).also {
            it.getAvailableAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.POPULARITY_DESC.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AddonCollectionProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.NAME,
        ).also {
            it.getAvailableAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.NAME.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AddonCollectionProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.NAME_DESC,
        ).also {
            it.getAvailableAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.NAME_DESC.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AddonCollectionProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.DATE_ADDED,
        ).also {
            it.getAvailableAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.DATE_ADDED.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AddonCollectionProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.DATE_ADDED_DESC,
        ).also {
            it.getAvailableAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.DATE_ADDED_DESC.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        Unit
    }

    @Test
    fun `collection user can be configured`() = runTest {
        val mockedClient = prepareClient()
        val collectionUser = "user123"
        val collectionName = "collection123"
        val provider = AddonCollectionProvider(
            testContext,
            client = mockedClient,
            collectionUser = collectionUser,
            collectionName = collectionName,
        )

        provider.getAvailableAddons()
        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/" +
                    "$collectionUser/collections/$collectionName/addons/" +
                    "?page_size=$PAGE_SIZE" +
                    "&sort=${SortOption.POPULARITY_DESC.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        assertEquals(
            COLLECTION_FILE_NAME.format("${collectionUser}_$collectionName"),
            provider.getCacheFileName(),
        )
    }

    @Test
    fun `default collection is used if not configured`() = runTest {
        val mockedClient = prepareClient()

        val provider = AddonCollectionProvider(
            testContext,
            client = mockedClient,
        )

        provider.getAvailableAddons()
        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/" +
                    "$DEFAULT_COLLECTION_USER/collections/$DEFAULT_COLLECTION_NAME/addons/" +
                    "?page_size=$PAGE_SIZE" +
                    "&sort=${SortOption.POPULARITY_DESC.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        assertEquals(COLLECTION_FILE_NAME.format(DEFAULT_COLLECTION_NAME), provider.getCacheFileName())
    }

    @Test
    fun `cache file name is sanitized`() = runTest {
        val mockedClient = prepareClient()
        val collectionUser = "../../user"
        val collectionName = "../collection"
        val provider = AddonCollectionProvider(
            testContext,
            client = mockedClient,
            collectionUser = collectionUser,
            collectionName = collectionName,
        )

        assertEquals(
            COLLECTION_FILE_NAME.format("user_collection"),
            provider.getCacheFileName(),
        )
    }

    private fun prepareClient(
        jsonResponse: String = loadResourceAsString("/collection_with_empty_values.json"),
        status: Int = 200,
    ): Client {
        val mockedClient = mock<Client>()
        val mockedResponse = mock<Response>()
        val mockedBody = mock<Response.Body>()
        whenever(mockedBody.string(any())).thenReturn(jsonResponse)
        whenever(mockedResponse.body).thenReturn(mockedBody)
        whenever(mockedResponse.status).thenReturn(status)
        whenever(mockedClient.fetch(any())).thenReturn(mockedResponse)
        return mockedClient
    }
}

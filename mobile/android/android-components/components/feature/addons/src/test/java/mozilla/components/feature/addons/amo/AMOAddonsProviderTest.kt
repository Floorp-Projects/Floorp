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
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.Date
import java.util.concurrent.TimeUnit

@RunWith(AndroidJUnit4::class)
class AMOAddonsProviderTest {

    @Test
    fun `getFeaturedAddons - with a successful status response must contain add-ons`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/collection.json"))
        val provider = AMOAddonsProvider(testContext, client = mockedClient)
        val addons = provider.getFeaturedAddons()
        val addon = addons.first()

        assertTrue(addons.isNotEmpty())
        assertAddonIsUBlockOrigin(addon)
    }

    @Test
    fun `getFeaturedAddons - with a successful status response must handle empty values`() = runTest {
        val client = prepareClient()
        val provider = AMOAddonsProvider(testContext, client = client)

        val addons = provider.getFeaturedAddons()
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
    fun `getFeaturedAddons - with a language`() = runTest {
        val client = prepareClient(loadResourceAsString("/localized_collection.json"))
        val provider = AMOAddonsProvider(testContext, client = client)

        val addons = provider.getFeaturedAddons(language = "en")
        val addon = addons.first()

        assertTrue(addons.isNotEmpty())

        // Add-on
        assertEquals("uBlock0@raymondhill.net", addon.id)
        assertEquals("2015-04-25T07:26:22Z", addon.createdAt)
        assertEquals("2021-02-01T14:04:16Z", addon.updatedAt)
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
    fun `getFeaturedAddons - read timeout can be configured`() = runTest {
        val mockedClient = prepareClient()

        val provider = spy(AMOAddonsProvider(testContext, client = mockedClient))
        provider.getFeaturedAddons(readTimeoutInSeconds = 5)
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
    fun `getFeaturedAddons - with unexpected status will throw exception`() = runTest {
        val mockedClient = prepareClient(status = 500)
        val provider = AMOAddonsProvider(testContext, client = mockedClient)
        provider.getFeaturedAddons()
        Unit
    }

    @Test
    fun `getFeaturedAddons - returns cached result if allowed and not expired`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/collection.json"))

        val provider = spy(AMOAddonsProvider(testContext, client = mockedClient))
        provider.getFeaturedAddons(false)
        verify(provider, never()).readFromDiskCache(null, useFallbackFile = false)

        whenever(provider.cacheExpired(testContext, null, useFallbackFile = false)).thenReturn(true)
        provider.getFeaturedAddons(true)
        verify(provider, never()).readFromDiskCache(null, useFallbackFile = false)

        whenever(provider.cacheExpired(testContext, null, useFallbackFile = false)).thenReturn(false)
        provider.getFeaturedAddons(true)
        verify(provider).readFromDiskCache(null, useFallbackFile = false)
        Unit
    }

    @Test
    fun `getFeaturedAddons - returns cached result if allowed and fetch failed`() = runTest {
        val mockedClient: Client = mock()
        val exception = IOException("test")
        val cachedAddons: List<Addon> = emptyList()
        whenever(mockedClient.fetch(any())).thenThrow(exception)

        val provider = spy(AMOAddonsProvider(testContext, client = mockedClient))

        try {
            // allowCache = false
            provider.getFeaturedAddons(allowCache = false)
            fail("Expected IOException")
        } catch (e: IOException) {
            assertSame(exception, e)
        }

        try {
            // allowCache = true, but no cache present
            provider.getFeaturedAddons(allowCache = true)
            fail("Expected IOException")
        } catch (e: IOException) {
            assertSame(exception, e)
        }

        try {
            // allowCache = true, cache present, but we fail to read
            whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(Date().time)
            provider.getFeaturedAddons(allowCache = true)
            fail("Expected IOException")
        } catch (e: IOException) {
            assertSame(exception, e)
        }

        // allowCache = true, cache present for a fallback file, and reading successfully
        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = true)).thenReturn(Date().time)
        whenever(provider.readFromDiskCache(null, useFallbackFile = true)).thenReturn(cachedAddons)
        assertSame(cachedAddons, provider.getFeaturedAddons(allowCache = true))

        // allowCache = true, cache present, and reading successfully
        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(Date().time)
        whenever(provider.cacheExpired(testContext, null, useFallbackFile = false)).thenReturn(false)
        whenever(provider.readFromDiskCache(null, useFallbackFile = false)).thenReturn(cachedAddons)
        assertSame(cachedAddons, provider.getFeaturedAddons(allowCache = true))
    }

    @Test
    fun `getFeaturedAddons - writes response to cache if configured`() = runTest {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = prepareClient(jsonResponse)

        val provider = spy(AMOAddonsProvider(testContext, client = mockedClient))
        val cachingProvider = spy(AMOAddonsProvider(testContext, client = mockedClient, maxCacheAgeInMinutes = 1))

        provider.getFeaturedAddons()
        verify(provider, never()).writeToDiskCache(jsonResponse, null)

        cachingProvider.getFeaturedAddons()
        verify(cachingProvider).writeToDiskCache(jsonResponse, null)
    }

    @Test
    fun `getFeaturedAddons - deletes unused cache files`() = runTest {
        val jsonResponse = loadResourceAsString("/collection.json")
        val mockedClient = prepareClient(jsonResponse)

        val provider = spy(AMOAddonsProvider(testContext, client = mockedClient, maxCacheAgeInMinutes = 1))

        provider.getFeaturedAddons()
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

        val provider = AMOAddonsProvider(testContext, client = prepareClient(), maxCacheAgeInMinutes = 1)
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

        val provider = AMOAddonsProvider(testContext, client = prepareClient(), maxCacheAgeInMinutes = 1)
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
        val provider = AMOAddonsProvider(testContext, client = prepareClient(), maxCacheAgeInMinutes = 1)
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
    fun `getFeaturedAddons - cache expiration check`() {
        var provider = spy(AMOAddonsProvider(testContext, client = mock(), maxCacheAgeInMinutes = -1))
        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(Date().time)
        assertTrue(provider.cacheExpired(testContext, null, useFallbackFile = false))

        whenever(provider.getCacheLastUpdated(testContext, null, useFallbackFile = false)).thenReturn(-1)
        assertTrue(provider.cacheExpired(testContext, null, useFallbackFile = false))

        provider = spy(AMOAddonsProvider(testContext, client = mock(), maxCacheAgeInMinutes = 10))
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

        val provider = AMOAddonsProvider(testContext, client = mockedClient)
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
        val provider = AMOAddonsProvider(testContext, client = mockedClient)
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
        val provider = AMOAddonsProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
        )

        provider.getFeaturedAddons()
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
        AMOAddonsProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.POPULARITY,
        ).also {
            it.getFeaturedAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.POPULARITY.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AMOAddonsProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.POPULARITY_DESC,
        ).also {
            it.getFeaturedAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.POPULARITY_DESC.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AMOAddonsProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.NAME,
        ).also {
            it.getFeaturedAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.NAME.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AMOAddonsProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.NAME_DESC,
        ).also {
            it.getFeaturedAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.NAME_DESC.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AMOAddonsProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.DATE_ADDED,
        ).also {
            it.getFeaturedAddons()
        }

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/accounts/account/mozilla/collections/" +
                    "$collectionName/addons/?page_size=$PAGE_SIZE&sort=${SortOption.DATE_ADDED.value}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )

        AMOAddonsProvider(
            testContext,
            client = mockedClient,
            collectionName = collectionName,
            sortOption = SortOption.DATE_ADDED_DESC,
        ).also {
            it.getFeaturedAddons()
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
        val provider = AMOAddonsProvider(
            testContext,
            client = mockedClient,
            collectionUser = collectionUser,
            collectionName = collectionName,
        )

        provider.getFeaturedAddons()
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

        val provider = AMOAddonsProvider(
            testContext,
            client = mockedClient,
        )

        provider.getFeaturedAddons()
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
        val provider = AMOAddonsProvider(
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

    @Test
    fun `getAddonsByGUIDs - with a successful status response must contain add-ons (single result)`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/amo_search_single_result.json"))
        val provider = AMOAddonsProvider(testContext, client = mockedClient)

        val addons = provider.getAddonsByGUIDs(listOf<String>("uBlock0@raymondhill.net"))

        // We should retrieve a single add-on.
        assertEquals(1, addons.size)
        // Verify that the add-on has been populated correctly.
        val addon = addons.first()
        assertAddonIsUBlockOrigin(addon)

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/addons/search/?guid=uBlock0@raymondhill.net",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )
    }

    @Test
    fun `getAddonsByGUIDs - with empty list of GUIDs`() = runTest {
        // It does not really matter we pass as resource since we don't expect any API call.
        val mockedClient = prepareClient(loadResourceAsString("/amo_search_single_result.json"))
        val provider = AMOAddonsProvider(testContext, client = mockedClient)

        val addons = provider.getAddonsByGUIDs(emptyList())

        assertTrue(addons.isEmpty())
        verifyNoInteractions(mockedClient)
    }

    @Test
    fun `getAddonsByGUIDs - with a language`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/amo_search_localized_single_result.json"))
        val provider = AMOAddonsProvider(testContext, client = mockedClient)
        val language = "ca"

        val addons = provider.getAddonsByGUIDs(
            guids = listOf<String>("uBlock0@raymondhill.net"),
            language = language,
        )

        // We should retrieve a single add-on.
        assertEquals(1, addons.size)
        // Verify that the add-on has been populated correctly.
        val addon = addons.first()
        assertEquals("uBlock0@raymondhill.net", addon.id)
        assertEquals("2015-04-25T07:26:22Z", addon.createdAt)
        assertEquals("2023-07-19T23:09:25Z", addon.updatedAt)
        assertEquals(
            "https://addons.mozilla.org/user-media/addon_icons/607/607454-64.png?modified=mcrushed",
            addon.iconUrl,
        )
        assertEquals(
            "https://addons.mozilla.org/ca/firefox/addon/ublock-origin/",
            addon.siteUrl,
        )
        assertEquals(
            "4141256",
            addon.downloadId,
        )
        assertEquals(
            "https://addons.mozilla.org/firefox/downloads/file/4141256/ublock_origin-1.51.0.xpi",
            addon.downloadUrl,
        )
        assertEquals(
            "dns",
            addon.permissions.first(),
        )
        assertEquals(
            "uBlock Origin",
            addon.translatableName[language],
        )
        assertEquals(
            "uBlock Origin",
            addon.translatableName[language],
        )

        assertEquals(
            "Finalment, un blocador eficient que utilitza pocs recursos de memòria i processador.",
            addon.translatableSummary[language],
        )

        assertTrue(addon.translatableDescription.getValue(language).isNotBlank())
        assertEquals("1.51.0", addon.version)
        assertEquals("ca", addon.defaultLocale)
        // Authors
        assertEquals("11423598", addon.authors.first().id)
        assertEquals("Raymond Hill", addon.authors.first().name)
        assertEquals("gorhill", addon.authors.first().username)
        assertEquals(
            "https://addons.mozilla.org/ca/firefox/user/11423598/",
            addon.authors.first().url,
        )
        // Ratings
        assertEquals(4.7825F, addon.rating!!.average, 0.7825F)
        assertEquals(15799, addon.rating!!.reviews)

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/addons/search/?guid=uBlock0@raymondhill.net&lang=ca",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )
    }

    @Test
    fun `getAddonsByGUIDs - with a successful status response must contain add-ons (multiple results)`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/amo_search_multiple_results.json"))
        val provider = AMOAddonsProvider(testContext, client = mockedClient)
        val guids = listOf<String>(
            "uBlock0@raymondhill.net",
            // Google Search Fixer
            "{58c32ac4-0d6c-4d6f-ae2c-96aaf8ffcb66}",
        )

        val addons = provider.getAddonsByGUIDs(guids)

        assertEquals(2, addons.size)
        // Verify that the first add-on has been populated correctly.
        var addon = addons.first()
        assertAddonIsUBlockOrigin(addon)

        // Verify the second add-on (Google Search Fixer)
        addon = addons.last()
        assertEquals("{58c32ac4-0d6c-4d6f-ae2c-96aaf8ffcb66}", addon.id)
        assertEquals("2017-10-31T15:35:56Z", addon.createdAt)
        assertEquals("2020-10-05T16:52:49Z", addon.updatedAt)
        assertEquals(
            "https://addons.mozilla.org/user-media/addon_icons/869/869140-64.png?modified=mcrushed",
            addon.iconUrl,
        )
        assertEquals(
            "https://addons.mozilla.org/en-US/firefox/addon/google-search-fixer/",
            addon.siteUrl,
        )
        assertEquals(
            "3655036",
            addon.downloadId,
        )
        assertEquals(
            "https://addons.mozilla.org/firefox/downloads/file/3655036/google_search_fixer-1.6.xpi",
            addon.downloadUrl,
        )
        assertEquals(
            "webRequest",
            addon.permissions.first(),
        )
        assertEquals(
            "Google Search Fixer",
            addon.translatableName["en-us"],
        )
        assertEquals(
            "Override the user-agent string presented to Google Search pages to receive the search experience shown to Chrome.",
            addon.translatableSummary["en-us"],
        )
        assertTrue(addon.translatableDescription.getValue("en-us").isNotBlank())
        assertEquals("1.6", addon.version)
        assertEquals("en-us", addon.defaultLocale)

        // Authors
        assertEquals("13394925", addon.authors.first().id)
        assertEquals("Thomas Wisniewski", addon.authors.first().name)
        assertEquals("wisniewskit", addon.authors.first().username)
        assertEquals(
            "https://addons.mozilla.org/en-US/firefox/user/13394925/",
            addon.authors.first().url,
        )
        assertEquals("6084813", addon.authors.last().id)
        assertEquals("Rob W", addon.authors.last().name)
        assertEquals("RobW", addon.authors.last().username)
        assertEquals(
            "https://addons.mozilla.org/en-US/firefox/user/6084813/",
            addon.authors.last().url,
        )
        // Ratings
        assertEquals(4.4096F, addon.rating!!.average, 0.4096F)
        assertEquals(1233, addon.rating!!.reviews)

        assertEquals(addon, provider.installedAddons[addon.id])

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/addons/search/?guid=" +
                    "uBlock0@raymondhill.net,{58c32ac4-0d6c-4d6f-ae2c-96aaf8ffcb66}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )
    }

    @Test
    fun `getAddonsByGUIDs - read timeout can be configured`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/amo_search_single_result.json"))
        val provider = spy(AMOAddonsProvider(testContext, client = mockedClient))

        provider.getAddonsByGUIDs(guids = listOf<String>("uBlock0@raymondhill.net"), readTimeoutInSeconds = 5)

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/addons/search/?guid=uBlock0@raymondhill.net",
                readTimeout = Pair(5, TimeUnit.SECONDS),
            ),
        )
    }

    @Test
    fun `getAddonsByGUIDs - read from the in-memory cache`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/amo_search_single_result.json"))
        val provider = AMOAddonsProvider(testContext, client = mockedClient)

        // In-memory cache should be empty by default.
        assertEquals(0, provider.installedAddons.size)

        // Add an add-on to the in-memory cache.
        val addon = Addon(
            id = "uBlock0@raymondhill.net",
            translatableName = mapOf("fr" to "ublock"),
        )
        provider.installedAddons[addon.id] = addon

        val addons = provider.getAddonsByGUIDs(guids = listOf("uBlock0@raymondhill.net"), language = "fr")

        assertEquals(1, addons.size)
        // The resulting add-on should be the one from the cache.
        assertEquals(addon, addons.first())
        // Make sure the in-memory cache has been updated.
        assertEquals(1, provider.installedAddons.size)

        verify(mockedClient, times(0)).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/addons/search/?guid=uBlock0@raymondhill.net&lang=fr",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )
        provider.installedAddons.clear()
    }

    @Test
    fun `getAddonsByGUIDs - skip in-memory cache when add-on doesn't have the right translation`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/amo_search_single_result.json"))
        val provider = AMOAddonsProvider(testContext, client = mockedClient)

        // In-memory cache should be empty by default.
        assertEquals(0, provider.installedAddons.size)

        // Add an add-on to the in-memory cache, which does not have a translatable name for `Addon.DEFAULT_LOCALE`.
        val addon = Addon(
            id = "uBlock0@raymondhill.net",
            translatableName = mapOf("fr" to "bloqueur de pubs"),
        )
        provider.installedAddons[addon.id] = addon

        val addons = provider.getAddonsByGUIDs(listOf("uBlock0@raymondhill.net"), language = "en-us")

        assertEquals(1, addons.size)
        assertEquals(addon.id, addons.first().id)
        // The name should be coming from the API response, not from the cached add-on.
        assertEquals(
            "uBlock Origin",
            addons.first().translatableName["en-us"],
        )
        // Make sure the in-memory cache has been updated.
        assertEquals(1, provider.installedAddons.size)

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/addons/search/?guid=uBlock0@raymondhill.net&lang=en-us",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )
        provider.installedAddons.clear()
    }

    @Test
    fun `getAddonsByGUIDs - skip in-memory cache when we don't have all the add-ons`() = runTest {
        val mockedClient = prepareClient(loadResourceAsString("/amo_search_multiple_results.json"))
        val provider = AMOAddonsProvider(testContext, client = mockedClient)
        val guids = listOf<String>(
            "uBlock0@raymondhill.net",
            // Google Search Fixer
            "{58c32ac4-0d6c-4d6f-ae2c-96aaf8ffcb66}",
        )

        // In-memory cache should be empty by default.
        assertEquals(0, provider.installedAddons.size)

        // Only the first add-on is cached.
        val addon = Addon(
            id = guids.first(),
            translatableName = mapOf(Addon.DEFAULT_LOCALE to "ublock"),
        )
        provider.installedAddons[addon.id] = addon

        val addons = provider.getAddonsByGUIDs(guids)

        assertEquals(2, addons.size)
        assertEquals("uBlock0@raymondhill.net", addons.first().id)
        assertEquals("{58c32ac4-0d6c-4d6f-ae2c-96aaf8ffcb66}", addons.last().id)
        // Make sure the in-memory cache has been updated.
        assertEquals(2, provider.installedAddons.size)

        verify(mockedClient).fetch(
            Request(
                url = "https://services.addons.mozilla.org/api/v4/addons/search/?guid=" +
                    "uBlock0@raymondhill.net,{58c32ac4-0d6c-4d6f-ae2c-96aaf8ffcb66}",
                readTimeout = Pair(DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS),
            ),
        )
        provider.installedAddons.clear()
    }

    private fun assertAddonIsUBlockOrigin(addon: Addon) {
        // Add-on details
        assertEquals("uBlock0@raymondhill.net", addon.id)
        assertEquals("2015-04-25T07:26:22Z", addon.createdAt)
        assertEquals("2023-07-19T23:09:25Z", addon.updatedAt)
        assertEquals(
            "https://addons.mozilla.org/user-media/addon_icons/607/607454-64.png?modified=mcrushed",
            addon.iconUrl,
        )
        assertEquals(
            "https://addons.mozilla.org/en-US/firefox/addon/ublock-origin/",
            addon.siteUrl,
        )
        assertEquals(
            "4141256",
            addon.downloadId,
        )
        assertEquals(
            "https://addons.mozilla.org/firefox/downloads/file/4141256/ublock_origin-1.51.0.xpi",
            addon.downloadUrl,
        )
        assertEquals(
            "dns",
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
        assertEquals("1.51.0", addon.version)
        assertEquals("en-us", addon.defaultLocale)
        // Authors
        assertEquals("11423598", addon.authors.first().id)
        assertEquals("Raymond Hill", addon.authors.first().name)
        assertEquals("gorhill", addon.authors.first().username)
        assertEquals(
            "https://addons.mozilla.org/en-US/firefox/user/11423598/",
            addon.authors.first().url,
        )
        // Ratings
        assertEquals(4.7825F, addon.rating!!.average, 0.7825F)
        assertEquals(15799, addon.rating!!.reviews)
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

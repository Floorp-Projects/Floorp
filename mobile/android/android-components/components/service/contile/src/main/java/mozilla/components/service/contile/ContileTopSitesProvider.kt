/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import android.content.Context
import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSitesProvider
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.asSequence
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.ktx.util.writeString
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.util.Date

internal const val CONTILE_ENDPOINT_URL = "https://contile.services.mozilla.com/v1/tiles"
internal const val CACHE_FILE_NAME = "mozilla_components_service_contile.json"
internal const val MINUTE_IN_MS = 60 * 1000

/**
 * Provides access to the Contile services API.
 *
 * @param context A reference to the application context.
 * @property client [Client] used for interacting with the Contile HTTP API.
 * @property endPointURL The url of the endpoint to fetch from. Defaults to [CONTILE_ENDPOINT_URL].
 * @property maxCacheAgeInMinutes Maximum time (in minutes) the cache should remain valid
 * before a refresh is attempted. Defaults to -1, meaning no cache is being used by default.
 */
class ContileTopSitesProvider(
    context: Context,
    private val client: Client,
    private val endPointURL: String = CONTILE_ENDPOINT_URL,
    private val maxCacheAgeInMinutes: Long = -1,
) : TopSitesProvider {

    private val applicationContext = context.applicationContext
    private val logger = Logger("ContileTopSitesProvider")
    private val diskCacheLock = Any()

    // The last modified time of the disk cache.
    @VisibleForTesting
    @Volatile
    internal var diskCacheLastModified: Long? = null

    /**
     * Fetches from the top sites [endPointURL] to provide a list of provided top sites.
     * Returns a cached response if [allowCache] is true and the cache is not expired
     * (@see [maxCacheAgeInMinutes]).
     *
     * @param allowCache Whether or not the result may be provided from a previously cached
     * response. Note that a [maxCacheAgeInMinutes] must be provided in order for the cache to be
     * active.
     * @throws IOException if the request failed to fetch any top sites.
     */
    @Throws(IOException::class)
    override suspend fun getTopSites(allowCache: Boolean): List<TopSite.Provided> {
        val cachedTopSites = if (allowCache && !isCacheExpired()) {
            readFromDiskCache()
        } else {
            null
        }

        if (!cachedTopSites.isNullOrEmpty()) {
            return cachedTopSites
        }

        return try {
            fetchTopSites()
        } catch (e: IOException) {
            logger.error("Failed to fetch contile top sites", e)
            throw e
        }
    }

    /**
     * Refreshes the cache with the latest top sites response from [endPointURL]
     * if the cache is active (@see [maxCacheAgeInMinutes]) and expired.
     */
    suspend fun refreshTopSitesIfCacheExpired() {
        if (maxCacheAgeInMinutes < 0 || !isCacheExpired()) return

        getTopSites(allowCache = false)
    }

    private fun fetchTopSites(): List<TopSite.Provided> {
        client.fetch(
            Request(url = endPointURL),
        ).use { response ->
            if (response.isSuccess) {
                val responseBody = response.body.string(Charsets.UTF_8)

                return try {
                    JSONObject(responseBody).getTopSites().also {
                        if (maxCacheAgeInMinutes > 0) {
                            writeToDiskCache(responseBody)
                        }
                    }
                } catch (e: JSONException) {
                    throw IOException(e)
                }
            } else {
                val errorMessage =
                    "Failed to fetch contile top sites. Status code: ${response.status}"
                logger.error(errorMessage)
                throw IOException(errorMessage)
            }
        }
    }

    @VisibleForTesting
    internal fun readFromDiskCache(): List<TopSite.Provided>? {
        synchronized(diskCacheLock) {
            return getCacheFile().readAndDeserialize {
                JSONObject(it).getTopSites()
            }
        }
    }

    @VisibleForTesting
    internal fun writeToDiskCache(responseBody: String) {
        synchronized(diskCacheLock) {
            getCacheFile().let {
                it.writeString { responseBody }
                diskCacheLastModified = System.currentTimeMillis()
            }
        }
    }

    @VisibleForTesting
    internal fun isCacheExpired() =
        getCacheLastModified() < Date().time - maxCacheAgeInMinutes * MINUTE_IN_MS

    @VisibleForTesting
    internal fun getCacheLastModified(): Long {
        diskCacheLastModified?.let { return it }

        val file = getBaseCacheFile()

        return if (file.exists()) {
            file.lastModified().also {
                diskCacheLastModified = it
            }
        } else {
            -1
        }
    }

    private fun getCacheFile(): AtomicFile = AtomicFile(getBaseCacheFile())

    @VisibleForTesting
    internal fun getBaseCacheFile(): File = File(applicationContext.filesDir, CACHE_FILE_NAME)
}

internal fun JSONObject.getTopSites(): List<TopSite.Provided> =
    getJSONArray("tiles")
        .asSequence { i -> getJSONObject(i) }
        .mapNotNull { it.toTopSite() }
        .toList()

private fun JSONObject.toTopSite(): TopSite.Provided? {
    return try {
        TopSite.Provided(
            id = getLong("id"),
            title = getString("name"),
            url = getString("url"),
            clickUrl = getString("click_url"),
            imageUrl = getString("image_url"),
            impressionUrl = getString("impression_url"),
            createdAt = null,
        )
    } catch (e: JSONException) {
        null
    }
}

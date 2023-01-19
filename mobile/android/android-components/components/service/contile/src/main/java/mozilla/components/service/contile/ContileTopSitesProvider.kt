/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import android.content.Context
import android.text.format.DateUtils
import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSitesProvider
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.asSequence
import mozilla.components.support.ktx.android.org.json.tryGetLong
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.ktx.util.writeString
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.util.Date

internal const val CONTILE_ENDPOINT_URL = "https://contile.services.mozilla.com/v1/tiles"
internal const val CACHE_FILE_NAME = "mozilla_components_service_contile.json"
internal const val CACHE_VALID_FOR_KEY = "valid_for"
internal const val CACHE_TOP_SITES_KEY = "tiles"

/**
 * Provides access to the Contile services API.
 *
 * @param context A reference to the application context.
 * @property client [Client] used for interacting with the Contile HTTP API.
 * @property endPointURL The url of the endpoint to fetch from. Defaults to [CONTILE_ENDPOINT_URL].
 * @property maxCacheAgeInSeconds Maximum time (in seconds) the cache should remain valid
 * before a refresh is attempted. Defaults to -1, meaning the max age defined by the server
 * will be used.
 */
class ContileTopSitesProvider(
    context: Context,
    private val client: Client,
    private val endPointURL: String = CONTILE_ENDPOINT_URL,
    private val maxCacheAgeInSeconds: Long = -1,
) : TopSitesProvider {

    private val applicationContext = context.applicationContext
    private val logger = Logger("ContileTopSitesProvider")
    private val diskCacheLock = Any()

    // Current state of the cache.
    @VisibleForTesting
    @Volatile
    internal var cacheState: CacheState = CacheState(
        shouldUseServerMaxAge = maxCacheAgeInSeconds == -1L,
    )

    /**
     * Fetches from the top sites [endPointURL] to provide a list of provided top sites.
     * Returns a cached response if [allowCache] is true and the cache is not expired
     * (@see [maxCacheAgeInSeconds]).
     *
     * @param allowCache Whether or not the result may be provided from a previously cached
     * response.
     * @throws IOException if the request failed to fetch any top sites.
     */
    @Throws(IOException::class)
    override suspend fun getTopSites(allowCache: Boolean): List<TopSite.Provided> {
        val cachedTopSites = if (allowCache && !isCacheExpired()) {
            readFromDiskCache()?.topSites
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
     * if the cache is expired.
     */
    suspend fun refreshTopSitesIfCacheExpired() {
        if (!isCacheExpired()) return

        getTopSites(allowCache = false)
    }

    private fun fetchTopSites(): List<TopSite.Provided> {
        client.fetch(
            Request(url = endPointURL),
        ).use { response ->
            if (response.isSuccess) {
                val responseBody = response.body.string(Charsets.UTF_8)

                if (response.status == Response.NO_CONTENT) {
                    // If the response is 204, we should invalidate the cached top sites
                    cacheState = cacheState.invalidate()
                    getCacheFile().delete()
                    return listOf()
                }

                return try {
                    val jsonBody = JSONObject(responseBody)
                    writeToDiskCache(
                        response.headers.computeValidFor() * DateUtils.SECOND_IN_MILLIS,
                        jsonBody.getJSONArray(CACHE_TOP_SITES_KEY),
                    )

                    jsonBody.getTopSites()
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
    internal fun readFromDiskCache(): CachedData? {
        synchronized(diskCacheLock) {
            return getCacheFile().readAndDeserialize {
                JSONObject(it).let { cachedObject ->
                    CachedData(cachedObject.validFor, cachedObject.getTopSites())
                }
            }
        }
    }

    /**
     * Write the validity time and top sites to a file for caching purposes.
     *
     * @param validFor Time in milliseconds describing the click validity for the set of top sites.
     * @param topSites [JSONArray] containing the top sites to be cached.
     */
    @VisibleForTesting
    internal fun writeToDiskCache(validFor: Long, topSites: JSONArray) {
        val cachedData = JSONObject().apply {
            put(CACHE_VALID_FOR_KEY, validFor)
            put(CACHE_TOP_SITES_KEY, topSites)
        }
        synchronized(diskCacheLock) {
            getCacheFile().let {
                it.writeString { cachedData.toString() }

                // Update the cache state to reflect the current status
                cacheState = cacheState.computeMaxAges(
                    System.currentTimeMillis(),
                    maxCacheAgeInSeconds * DateUtils.SECOND_IN_MILLIS,
                    validFor,
                )
            }
        }
    }

    @VisibleForTesting
    internal fun isCacheExpired(): Boolean {
        cacheState.cacheMaxAge?.let { return Date().time > it }

        val file = getBaseCacheFile()

        cacheState =
            if (file.exists()) {
                cacheState.computeMaxAges(
                    file.lastModified(),
                    maxCacheAgeInSeconds * DateUtils.SECOND_IN_MILLIS,
                    (readFromDiskCache()?.validFor ?: 0L) * DateUtils.SECOND_IN_MILLIS,
                )
            } else {
                cacheState.invalidate()
            }

        // If cache is invalid, we should also consider it as expired
        return Date().time > (cacheState.cacheMaxAge ?: -1L)
    }

    private fun getCacheFile(): AtomicFile = AtomicFile(getBaseCacheFile())

    @VisibleForTesting
    internal fun getBaseCacheFile(): File = File(applicationContext.filesDir, CACHE_FILE_NAME)

    /**
     * Data stored in the cache file
     *
     * @param validFor Time in milliseconds describing the click validity for the set of top sites.
     * @param topSites List of provided top sites.
     */
    internal data class CachedData(
        val validFor: Long,
        val topSites: List<TopSite.Provided>,
    )

    /**
     * Current state of the cache.
     *
     * @param shouldUseServerMaxAge Whether or not [serverCacheMaxAge] should be used instead of
     * [localCacheMaxAge].
     * @param isCacheValid Whether or not the current set of cached top sites is still valid.
     * @param localCacheMaxAge Maximum unix timestamp until the current set of cached top sites
     * is still valid, specified by the client.
     * @param serverCacheMaxAge Maximum unix timestamp until the current set of cached top sites
     * is still valid, specified by the server.
     */
    internal data class CacheState(
        val shouldUseServerMaxAge: Boolean,
        val isCacheValid: Boolean = true,
        val localCacheMaxAge: Long? = null,
        val serverCacheMaxAge: Long? = null,
    ) {
        val cacheMaxAge
            get() = if (isCacheValid) {
                if (shouldUseServerMaxAge) serverCacheMaxAge else localCacheMaxAge
            } else {
                null
            }

        fun invalidate(): CacheState =
            this.copy(isCacheValid = false, localCacheMaxAge = null, serverCacheMaxAge = null)

        /**
         * Update local and server max age values
         */
        fun computeMaxAges(lastModified: Long, localMaxAge: Long, serverMaxAge: Long): CacheState =
            this.copy(
                isCacheValid = true,
                localCacheMaxAge = lastModified + localMaxAge,
                serverCacheMaxAge = lastModified + serverMaxAge,
            )
    }
}

/**
 * To extract the `valid-for` value for the set of provided top sites, we need to sum up the `max-age`
 * and `stale-if-error` options from the header. These values can be found in the `cache-control` header,
 * formatted as `max-age=$value` and `stale-if-error=$value`.
 */
internal fun Headers.computeValidFor(): Long =
    getAll("cache-control").sumOf {
        val valueList = it.split("=")
            .map { item -> item.trim() }

        if (valueList.size == 2 && valueList[0] in listOf("max-age", "stale-if-error")) {
            valueList[1].toLong()
        } else {
            0L
        }
    }

internal fun JSONObject.getTopSites(): List<TopSite.Provided> =
    getJSONArray(CACHE_TOP_SITES_KEY)
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

internal val JSONObject.validFor: Long
    get() = this.tryGetLong(CACHE_VALID_FOR_KEY) ?: 0L

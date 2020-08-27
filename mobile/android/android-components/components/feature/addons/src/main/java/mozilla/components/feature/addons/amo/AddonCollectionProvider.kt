/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package mozilla.components.feature.addons.amo

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonsProvider
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.sanitizeURL
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.ktx.util.writeString
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.text.SimpleDateFormat
import java.util.concurrent.TimeUnit
import java.util.Date
import java.util.Locale

internal const val API_VERSION = "api/v4"
internal const val DEFAULT_SERVER_URL = "https://addons.mozilla.org"
internal const val DEFAULT_COLLECTION_NAME = "7e8d6dc651b54ab385fb8791bf9dac"
internal const val COLLECTION_FILE_NAME = "mozilla_components_addon_collection_%s.json"
internal const val MINUTE_IN_MS = 60 * 1000
internal const val DEFAULT_READ_TIMEOUT_IN_SECONDS = 20L

/**
 * Provide access to the AMO collections API.
 * https://addons-server.readthedocs.io/en/latest/topics/api/collections.html
 *
 * @property serverURL The url of the endpoint to interact with e.g production, staging
 * or testing. Defaults to [DEFAULT_SERVER_URL].
 * @property collectionName The name of the collection to access, defaults
 * to [DEFAULT_COLLECTION_NAME].
 * @property maxCacheAgeInMinutes maximum time (in minutes) the collection cache
 * should remain valid before a refresh is attempted. Defaults to -1, meaning no
 * cache is being used by default.
 * @property client A reference of [Client] for interacting with the AMO HTTP api.
 */
class AddonCollectionProvider(
    private val context: Context,
    private val client: Client,
    private val serverURL: String = DEFAULT_SERVER_URL,
    private val collectionName: String = DEFAULT_COLLECTION_NAME,
    private val maxCacheAgeInMinutes: Long = -1
) : AddonsProvider {

    private val logger = Logger("AddonCollectionProvider")

    private val diskCacheLock = Any()

    /**
     * Interacts with the collections endpoint to provide a list of available
     * add-ons. May return a cached response, if [allowCache] is true, and the
     * cache is not expired (see [maxCacheAgeInMinutes]) or fetching from
     * AMO failed.
     *
     * @param allowCache whether or not the result may be provided
     * from a previously cached response, defaults to true. Note that
     * [maxCacheAgeInMinutes] must be set for the cache to be active.
     * @param readTimeoutInSeconds optional timeout in seconds to use when fetching
     * available add-ons from a remote endpoint. If not specified [DEFAULT_READ_TIMEOUT_IN_SECONDS]
     * will be used.
     * @throws IOException if the request failed, or could not be executed due to cancellation,
     * a connectivity problem or a timeout.
     */
    @Throws(IOException::class)
    @Suppress("NestedBlockDepth")
    override suspend fun getAvailableAddons(allowCache: Boolean, readTimeoutInSeconds: Long?): List<Addon> {
        val cachedAvailableAddons = if (allowCache && !cacheExpired(context)) {
            readFromDiskCache()
        } else {
            null
        }

        if (cachedAvailableAddons != null) {
            return cachedAvailableAddons
        }

        return try {
            fetchAvailableAddons(readTimeoutInSeconds)
        } catch (e: IOException) {
            logger.error("Failed to fetch available add-ons", e)
            if (allowCache) {
                val cacheLastUpdated = getCacheLastUpdated(context)
                if (cacheLastUpdated > -1) {
                    val cache = readFromDiskCache()
                    cache?.let {
                        logger.info("Falling back to available add-ons cache from ${
                            SimpleDateFormat("yyyy-MM-dd'T'HH:mm'Z'", Locale.US).format(cacheLastUpdated)
                        }")
                        return it
                    }
                }
            }
            throw e
        }
    }

    private fun fetchAvailableAddons(readTimeoutInSeconds: Long?): List<Addon> {
        client.fetch(
            Request(
                url = "$serverURL/$API_VERSION/accounts/account/mozilla/collections/$collectionName/addons",
                readTimeout = Pair(readTimeoutInSeconds ?: DEFAULT_READ_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS)
            )
        )
        .use { response ->
            if (response.isSuccess) {
                val responseBody = response.body.string(Charsets.UTF_8)
                return try {
                    JSONObject(responseBody).getAddons().also {
                        if (maxCacheAgeInMinutes > 0) {
                            writeToDiskCache(responseBody)
                        }
                    }
                } catch (e: JSONException) {
                    throw IOException(e)
                }
            } else {
                val errorMessage = "Failed to fetch add-on collection. Status code: ${response.status}"
                logger.error(errorMessage)
                throw IOException(errorMessage)
            }
        }
    }

    /**
     * Fetches given Addon icon from the url and returns a decoded Bitmap
     * @throws IOException if the request could not be executed due to cancellation,
     * a connectivity problem or a timeout.
     */
    @Throws(IOException::class)
    suspend fun getAddonIconBitmap(addon: Addon): Bitmap? {
        var bitmap: Bitmap? = null
        if (addon.iconUrl != "") {
            client.fetch(
                    Request(url = addon.iconUrl.sanitizeURL())
            ).use { response ->
                if (response.isSuccess) {
                    response.body.useStream {
                        bitmap = BitmapFactory.decodeStream(it)
                    }
                }
            }
        }

        return bitmap
    }

    @VisibleForTesting
    internal fun writeToDiskCache(collectionResponse: String) {
        synchronized(diskCacheLock) {
            getCacheFile(context).writeString { collectionResponse }
        }
    }

    @VisibleForTesting
    internal fun readFromDiskCache(): List<Addon>? {
        synchronized(diskCacheLock) {
            return getCacheFile(context).readAndDeserialize {
                JSONObject(it).getAddons()
            }
        }
    }

    @VisibleForTesting
    internal fun cacheExpired(context: Context): Boolean {
        return getCacheLastUpdated(context) < Date().time - maxCacheAgeInMinutes * MINUTE_IN_MS
    }

    @VisibleForTesting
    internal fun getCacheLastUpdated(context: Context): Long {
        val file = getBaseCacheFile(context)
        return if (file.exists()) file.lastModified() else -1
    }

    private fun getCacheFile(context: Context): AtomicFile {
        return AtomicFile(getBaseCacheFile(context))
    }

    private fun getBaseCacheFile(context: Context): File {
        return File(context.filesDir, COLLECTION_FILE_NAME.format(collectionName))
    }
}

internal fun JSONObject.getAddons(): List<Addon> {
    val addonsJson = getJSONArray("results")
    return (0 until addonsJson.length()).map { index ->
        addonsJson.getJSONObject(index).toAddons()
    }
}

internal fun JSONObject.toAddons(): Addon {
    return with(getJSONObject("addon")) {
        Addon(
            id = getSafeString("guid"),
            authors = getAuthors(),
            categories = getCategories(),
            createdAt = getSafeString("created"),
            updatedAt = getSafeString("last_updated"),
            downloadUrl = getDownloadUrl(),
            version = getCurrentVersion(),
            permissions = getPermissions(),
            translatableName = getSafeMap("name"),
            translatableDescription = getSafeMap("description"),
            translatableSummary = getSafeMap("summary"),
            iconUrl = getSafeString("icon_url"),
            siteUrl = getSafeString("url"),
            rating = getRating(),
            defaultLocale = getSafeString("default_locale").ifEmpty { Addon.DEFAULT_LOCALE }
        )
    }
}

internal fun JSONObject.getRating(): Addon.Rating? {
    val jsonRating = optJSONObject("ratings")
    return if (jsonRating != null) {
        Addon.Rating(
            reviews = jsonRating.optInt("count"),
            average = jsonRating.optDouble("average").toFloat()
        )
    } else {
        null
    }
}

internal fun JSONObject.getCategories(): List<String> {
    val jsonCategories = optJSONObject("categories")
    return if (jsonCategories == null) {
        emptyList()
    } else {
        val jsonAndroidCategories = jsonCategories.getSafeJSONArray("android")
        (0 until jsonAndroidCategories.length()).map { index ->
            jsonAndroidCategories.getString(index)
        }
    }
}

internal fun JSONObject.getPermissions(): List<String> {
    val fileJson = getJSONObject("current_version")
        .getSafeJSONArray("files")
        .getJSONObject(0)

    val permissionsJson = fileJson.getSafeJSONArray("permissions")
    return (0 until permissionsJson.length()).map { index ->
        permissionsJson.getString(index)
    }
}

internal fun JSONObject.getCurrentVersion(): String {
    return optJSONObject("current_version")?.getSafeString("version") ?: ""
}

internal fun JSONObject.getDownloadUrl(): String {
    return (getJSONObject("current_version")
        .optJSONArray("files")
        ?.getJSONObject(0))
        ?.getSafeString("url") ?: ""
}

internal fun JSONObject.getAuthors(): List<Addon.Author> {
    val authorsJson = getSafeJSONArray("authors")
    return (0 until authorsJson.length()).map { index ->
        val authorJson = authorsJson.getJSONObject(index)

        Addon.Author(
            id = authorJson.getSafeString("id"),
            name = authorJson.getSafeString("name"),
            username = authorJson.getSafeString("username"),
            url = authorJson.getSafeString("url")
        )
    }
}

internal fun JSONObject.getSafeString(key: String): String {
    return if (isNull(key)) {
        ""
    } else {
        getString(key)
    }
}

internal fun JSONObject.getSafeJSONArray(key: String): JSONArray {
    return if (isNull(key)) {
        JSONArray("[]")
    } else {
        getJSONArray(key)
    }
}

internal fun JSONObject.getSafeMap(valueKey: String): Map<String, String> {
    return if (isNull(valueKey)) {
        emptyMap()
    } else {
        val map = mutableMapOf<String, String>()
        val jsonObject = getJSONObject(valueKey)

        jsonObject.keys()
            .forEach { key ->
                map[key] = jsonObject.getSafeString(key)
            }
        map
    }
}

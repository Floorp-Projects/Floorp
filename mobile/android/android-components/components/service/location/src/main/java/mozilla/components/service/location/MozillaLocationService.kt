/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.location

import android.content.Context
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.NONE
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.Build
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlin.sanitizeURL
import org.json.JSONException
import org.json.JSONObject
import java.io.IOException
import java.util.concurrent.TimeUnit

private const val GEOIP_SERVICE_URL = "https://location.services.mozilla.com/v1/"
private const val CONNECT_TIMEOUT_SECONDS = 10L
private const val READ_TIMEOUT_SECONDS = 10L
private const val USER_AGENT = "MozAC/" + Build.version
private const val EMPTY_REQUEST_BODY = "{}"
private const val CACHE_FILE = "mozac.service.location.region"
private const val KEY_COUNTRY_CODE = "country_code"
private const val KEY_COUNTRY_NAME = "country_name"

/**
 * The Mozilla Location Service (MLS) is an open service which lets devices determine their location
 * based on network infrastructure like Bluetooth beacons, cell towers and WiFi access points.
 *
 * - https://location.services.mozilla.com/
 * - https://mozilla.github.io/ichnaea/api/index.html
 *
 * Note: Accessing the Mozilla Location Service requires an API token:
 * https://location.services.mozilla.com/contact
 *
 * @param client The HTTP client that this [MozillaLocationService] should use for requests.
 * @param apiKey The API key that is used to access the Mozilla location service.
 * @param serviceUrl An optional URL override usually only needed for testing.
 */
class MozillaLocationService(
    private val context: Context,
    private val client: Client,
    apiKey: String,
    serviceUrl: String = GEOIP_SERVICE_URL,
) : LocationService {
    private val regionServiceUrl = (serviceUrl + "country?key=%s").format(apiKey)

    /**
     * Determines the current [LocationService.Region] based on the IP address used to access the service.
     *
     * https://mozilla.github.io/ichnaea/api/region.html
     *
     * @param readFromCache Whether a previously returned region (from the cache) can be returned
     * (default) or whether a request to the service should always be made.
     */
    override suspend fun fetchRegion(
        readFromCache: Boolean,
    ): LocationService.Region? = withContext(Dispatchers.IO) {
        if (readFromCache) {
            context.loadCachedRegion()?.let { return@withContext it }
        }

        client.fetchRegion(regionServiceUrl)?.also { context.cacheRegion(it) }
    }

    /**
     * Get if there is already a cached region.
     * This does not guarantee we have the current actual region but only the last value
     * which may be obsolete at this time.
     */
    override fun hasRegionCached(): Boolean {
        return context.hasCachedRegion()
    }
}

private fun Context.loadCachedRegion(): LocationService.Region? {
    val cache = regionCache()

    return if (cache.contains(KEY_COUNTRY_CODE) && cache.contains(KEY_COUNTRY_NAME)) {
        LocationService.Region(
            cache.getString(KEY_COUNTRY_CODE, null)!!,
            cache.getString(KEY_COUNTRY_NAME, null)!!,
        )
    } else {
        null
    }
}

private fun Context.hasCachedRegion(): Boolean {
    val cache = regionCache()
    return cache.contains(KEY_COUNTRY_CODE) && cache.contains(KEY_COUNTRY_NAME)
}

private fun Context.cacheRegion(region: LocationService.Region) {
    regionCache()
        .edit()
        .putString(KEY_COUNTRY_CODE, region.countryCode)
        .putString(KEY_COUNTRY_NAME, region.countryName)
        .apply()
}

@VisibleForTesting(otherwise = NONE)
internal fun Context.clearRegionCache() {
    regionCache()
        .edit()
        .clear()
        .apply()
}

private fun Context.regionCache(): SharedPreferences {
    return getSharedPreferences(CACHE_FILE, Context.MODE_PRIVATE)
}

private fun Client.fetchRegion(regionServiceUrl: String): LocationService.Region? {
    val request = Request(
        url = regionServiceUrl.sanitizeURL(),
        method = Request.Method.POST,
        headers = MutableHeaders(
            Headers.Names.CONTENT_TYPE to Headers.Values.CONTENT_TYPE_APPLICATION_JSON,
            Headers.Names.USER_AGENT to USER_AGENT,
        ),
        // We are posting an empty request body here. This means the service will only use the IP
        // address to provide a region response. Technically it's possible to also provide data
        // about nearby Bluetooth, cell or WiFi networks.
        body = Request.Body.fromString(EMPTY_REQUEST_BODY),
        connectTimeout = Pair(CONNECT_TIMEOUT_SECONDS, TimeUnit.SECONDS),
        readTimeout = Pair(READ_TIMEOUT_SECONDS, TimeUnit.SECONDS),
    )

    return try {
        fetch(request).toRegion()
    } catch (e: IOException) {
        Logger.debug(message = "Could not fetch region from location service", throwable = e)
        null
    }
}

private fun Response.toRegion(): LocationService.Region? {
    if (!isSuccess) {
        return null
    }

    use {
        return try {
            val json = JSONObject(body.string(Charsets.UTF_8))
            LocationService.Region(
                json.getString(KEY_COUNTRY_CODE),
                json.getString(KEY_COUNTRY_NAME),
            )
        } catch (e: JSONException) {
            Logger.debug(message = "Could not parse JSON returned from location service", throwable = e)
            null
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.api

import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Request.Body
import mozilla.components.concept.fetch.Request.Method
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.service.pocket.BuildConfig
import mozilla.components.service.pocket.PocketStoriesRequestConfig
import mozilla.components.service.pocket.ext.fetchBodyOrNull
import mozilla.components.service.pocket.logger
import mozilla.components.service.pocket.spocs.api.SpocsEndpointRaw.Companion.newInstance
import mozilla.components.service.pocket.stories.api.PocketEndpointRaw.Companion.newInstance
import org.json.JSONObject
import java.io.IOException
import java.util.UUID

private const val SPOCS_ENDPOINT_DEV_BASE_URL = "https://spocs.getpocket.dev/"
private const val SPOCS_ENDPOINT_PROD_BASE_URL = "https://spocs.getpocket.com/"
private const val SPOCS_ENDPOINT_DOWNLOAD_SPOCS_PATH = "spocs"
private const val SPOCS_ENDPOINT_DELETE_PROFILE_PATH = "user"
private const val SPOCS_PROXY_VERSION_KEY = "version"
private const val SPOCS_PROXY_VERSION_VALUE = 2
private const val SPOCS_PROXY_PROFILE_KEY = "pocket_id"
private const val SPOCS_PROXY_APP_KEY = "consumer_key"
private const val SPOCS_PROXY_SITE_KEY = "site"
private const val SPOCS_PROXY_COUNTRY_KEY = "country"
private const val SPOCS_PROXY_CITY_KEY = "city"

/**
 * Makes requests to the Pocket endpoint and returns the raw JSON data.
 *
 * @see [SpocsEndpoint], which wraps this to make it more practical.
 * @see [newInstance] to retrieve an instance.
 */
internal class SpocsEndpointRaw internal constructor(
    @get:VisibleForTesting internal val client: Client,
    @get:VisibleForTesting internal val profileId: UUID,
    @get:VisibleForTesting internal val appId: String,
    @get:VisibleForTesting internal val sponsoredStoriesParams: PocketStoriesRequestConfig,
) {
    /**
     * Gets the current sponsored stories recommendations from the Pocket server.
     *
     * @return The stories recommendations as a raw JSON string or null on error.
     */
    @WorkerThread
    fun getSponsoredStories(): String? {
        val url = Uri.Builder()
            .encodedPath(baseUrl + SPOCS_ENDPOINT_DOWNLOAD_SPOCS_PATH)
        if (sponsoredStoriesParams.siteId.isNotBlank()) {
            url.appendQueryParameter(SPOCS_PROXY_SITE_KEY, sponsoredStoriesParams.siteId)
        }
        if (sponsoredStoriesParams.country.isNotBlank()) {
            url.appendQueryParameter(SPOCS_PROXY_COUNTRY_KEY, sponsoredStoriesParams.country)
        }
        if (sponsoredStoriesParams.city.isNotBlank()) {
            url.appendQueryParameter(SPOCS_PROXY_CITY_KEY, sponsoredStoriesParams.city)
        }
        url.build()

        val request = Request(
            url = url.toString(),
            method = Method.POST,
            headers = getRequestHeaders(),
            body = getDownloadStoriesRequestBody(),
            conservative = true,
        )
        return client.fetchBodyOrNull(request)
    }

    /**
     * Request to delete all data stored on server about [profileId].
     *
     * @return [Boolean] indicating whether the delete operation was successful or not.
     */
    @WorkerThread
    fun deleteProfile(): Boolean {
        val url = Uri.Builder()
            .encodedPath(baseUrl + SPOCS_ENDPOINT_DELETE_PROFILE_PATH)
        if (sponsoredStoriesParams.siteId.isNotBlank()) {
            url.appendQueryParameter(SPOCS_PROXY_SITE_KEY, sponsoredStoriesParams.siteId)
        }
        if (sponsoredStoriesParams.country.isNotBlank()) {
            url.appendQueryParameter(SPOCS_PROXY_COUNTRY_KEY, sponsoredStoriesParams.country)
        }
        if (sponsoredStoriesParams.city.isNotBlank()) {
            url.appendQueryParameter(SPOCS_PROXY_CITY_KEY, sponsoredStoriesParams.city)
        }
        url.build()

        val request = Request(
            url = url.toString(),
            method = Method.DELETE,
            headers = getRequestHeaders(),
            body = getDeleteProfileRequestBody(),
            conservative = true,
        )

        val response: Response? = try {
            client.fetch(request)
        } catch (e: IOException) {
            logger.debug("Network error", e)
            null
        }

        return response?.isSuccess ?: false
    }

    private fun getRequestHeaders() = MutableHeaders(
        "Content-Type" to "application/json; charset=UTF-8",
        "Accept" to "*/*",
    )

    private fun getDownloadStoriesRequestBody(): Body {
        val params = mapOf(
            SPOCS_PROXY_VERSION_KEY to SPOCS_PROXY_VERSION_VALUE,
            SPOCS_PROXY_PROFILE_KEY to profileId.toString(),
            SPOCS_PROXY_APP_KEY to appId,
        )

        return Body(JSONObject(params).toString().byteInputStream())
    }

    private fun getDeleteProfileRequestBody(): Body {
        val params = mapOf(
            SPOCS_PROXY_PROFILE_KEY to profileId.toString(),
        )

        return Body(JSONObject(params).toString().byteInputStream())
    }

    companion object {
        /**
         * Returns a new instance of [SpocsEndpointRaw].
         *
         * @param client HTTP client to use for network requests.
         * @param profileId Unique profile identifier which will be presented with sponsored stories.
         * @param appId Unique identifier of the application using this feature.
         * @param sponsoredStoriesParams Configuration containing parameters used to get the spoc content.
         */
        fun newInstance(
            client: Client,
            profileId: UUID,
            appId: String,
            sponsoredStoriesParams: PocketStoriesRequestConfig,
        ): SpocsEndpointRaw {
            return SpocsEndpointRaw(client, profileId, appId, sponsoredStoriesParams)
        }

        /**
         * Convenience for checking whether the current build is a debug build and overwriting this in tests.
         */
        @VisibleForTesting
        internal var isDebugBuild = BuildConfig.DEBUG

        /**
         * Get the base url for sponsored stories specific to development or production.
         */
        @VisibleForTesting
        internal val baseUrl
            get() = if (isDebugBuild) {
                SPOCS_ENDPOINT_DEV_BASE_URL
            } else {
                SPOCS_ENDPOINT_PROD_BASE_URL
            }
    }
}

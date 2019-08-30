/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs.verify

import android.webkit.URLUtil
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import org.json.JSONException
import org.json.JSONObject
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * Digital Asset Links APIs allows any caller to check pre declared
 * relationships between two assets which can be either web domains
 * or native applications. This requests checks for a specific
 * relationship declared by a web site with an Android application
 */
internal class DigitalAssetLinksHandler(
    private val httpClient: Client,
    private val apiKey: String?
) {

    fun checkDigitalAssetLinkRelationship(
        webDomain: String,
        packageName: String,
        fingerprint: String?,
        relationship: String
    ): Boolean {
        val requestUrl = getUrlForCheckingRelationship(webDomain, packageName, fingerprint, relationship, apiKey)
        val response = fetch(requestUrl) ?: return false

        val responseBody = response.use { response.body.string() }
        return try {
            val responseJson = JSONObject(responseBody)
            responseJson.getBoolean(DIGITAL_ASSET_LINKS_CHECK_RESPONSE_KEY_LINKED)
        } catch (e: JSONException) {
            false
        }
    }

    private fun fetch(requestUrl: String): Response? {
        if (!URLUtil.isValidUrl(requestUrl)) return null
        return try {
            val response = httpClient.fetch(
                Request(
                    requestUrl,
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT
                )
            )
            if (response.status == HTTP_OK) response else null
        } catch (e: IOException) {
            null
        }
    }

    companion object {
        private const val URL = "https://digitalassetlinks.googleapis.com/v1/assetlinks:check?"
        private const val TARGET_ORIGIN_PARAM = "source.web.site"
        private const val SOURCE_PACKAGE_NAME_PARAM = "target.androidApp.packageName"
        private const val SOURCE_PACKAGE_FINGERPRINT_PARAM = "target.androidApp.certificate.sha256Fingerprint"
        private const val RELATIONSHIP_PARAM = "relation"
        private const val PRETTY_PRINT_PARAM = "prettyPrint"
        private const val KEY_PARAM = "key"
        private const val DIGITAL_ASSET_LINKS_CHECK_RESPONSE_KEY_LINKED = "linked"
        private const val HTTP_OK = 200
        private val TIMEOUT = 3L to TimeUnit.SECONDS

        /**
         * Returns a URL used to check whether the specified (directional) relationship exists
         * between the specified source and target assets.
         *
         * https://developers.google.com/digital-asset-links/reference/rest/v1/assetlinks/check
         */
        @VisibleForTesting
        internal fun getUrlForCheckingRelationship(
            webDomain: String,
            packageName: String,
            fingerprint: String?,
            relationship: String,
            key: String?
        ) = URL.toUri().buildUpon()
            .appendQueryParameter(TARGET_ORIGIN_PARAM, webDomain)
            .appendQueryParameter(SOURCE_PACKAGE_NAME_PARAM, packageName)
            .appendQueryParameter(SOURCE_PACKAGE_FINGERPRINT_PARAM, fingerprint)
            .appendQueryParameter(RELATIONSHIP_PARAM, relationship)
            .appendQueryParameter(PRETTY_PRINT_PARAM, false.toString())
            .appendQueryParameter(KEY_PARAM, key.orEmpty())
            .build()
            .toString()
    }
}

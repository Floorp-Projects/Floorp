/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.api

import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.Relation
import mozilla.components.service.digitalassetlinks.RelationChecker
import mozilla.components.service.digitalassetlinks.Statement
import mozilla.components.service.digitalassetlinks.StatementListFetcher
import mozilla.components.service.digitalassetlinks.TIMEOUT
import mozilla.components.service.digitalassetlinks.ext.parseJsonBody
import mozilla.components.service.digitalassetlinks.ext.safeFetch
import mozilla.components.support.ktx.kotlin.sanitizeURL
import org.json.JSONObject

/**
 * Digital Asset Links allows any caller to check pre declared relationships between
 * two assets which can be either web domains or native applications.
 * This class checks for a specific relationship declared by two assets via the online API.
 */
class DigitalAssetLinksApi(
    private val httpClient: Client,
    private val apiKey: String?,
) : RelationChecker, StatementListFetcher {

    override fun checkRelationship(
        source: AssetDescriptor.Web,
        relation: Relation,
        target: AssetDescriptor,
    ): Boolean {
        val request = buildCheckApiRequest(source, relation, target)
        val response = httpClient.safeFetch(request)
        val parsed = response?.parseJsonBody { body ->
            parseCheckAssetLinksJson(JSONObject(body))
        }
        return parsed?.linked == true
    }

    override fun listStatements(source: AssetDescriptor.Web): Sequence<Statement> {
        val request = buildListApiRequest(source)
        val response = httpClient.safeFetch(request)
        val parsed = response?.parseJsonBody { body ->
            parseListStatementsJson(JSONObject(body))
        }
        return parsed?.statements.orEmpty().asSequence()
    }

    private fun apiUrlBuilder(path: String) = BASE_URL.toUri().buildUpon()
        .encodedPath(path)
        .appendQueryParameter("prettyPrint", false.toString())
        .appendQueryParameter("key", apiKey)

    /**
     * Returns a [Request] used to check whether the specified (directional) relationship exists
     * between the specified source and target assets.
     *
     * https://developers.google.com/digital-asset-links/reference/rest/v1/assetlinks/check
     */
    @VisibleForTesting
    internal fun buildCheckApiRequest(
        source: AssetDescriptor,
        relation: Relation,
        target: AssetDescriptor,
    ): Request {
        val uriBuilder = apiUrlBuilder(CHECK_PATH)
            .appendQueryParameter("relation", relation.kindAndDetail)

        // source and target follow the same format, so re-use the query logic for both.
        uriBuilder.appendAssetAsQuery(source, "source")
        uriBuilder.appendAssetAsQuery(target, "target")

        return Request(
            url = uriBuilder.build().toString().sanitizeURL(),
            method = Request.Method.GET,
            connectTimeout = TIMEOUT,
            readTimeout = TIMEOUT,
        )
    }

    @VisibleForTesting
    internal fun buildListApiRequest(source: AssetDescriptor): Request {
        val uriBuilder = apiUrlBuilder(LIST_PATH)
        uriBuilder.appendAssetAsQuery(source, "source")

        return Request(
            url = uriBuilder.build().toString().sanitizeURL(),
            method = Request.Method.GET,
            connectTimeout = TIMEOUT,
            readTimeout = TIMEOUT,
        )
    }

    private fun Uri.Builder.appendAssetAsQuery(asset: AssetDescriptor, prefix: String) {
        when (asset) {
            is AssetDescriptor.Web -> {
                appendQueryParameter("$prefix.web.site", asset.site)
            }
            is AssetDescriptor.Android -> {
                appendQueryParameter("$prefix.androidApp.packageName", asset.packageName)
                appendQueryParameter(
                    "$prefix.androidApp.certificate.sha256Fingerprint",
                    asset.sha256CertFingerprint,
                )
            }
        }
    }

    companion object {
        private const val BASE_URL = "https://digitalassetlinks.googleapis.com"
        private const val CHECK_PATH = "/v1/assetlinks:check"
        private const val LIST_PATH = "/v1/statements:list"
    }
}

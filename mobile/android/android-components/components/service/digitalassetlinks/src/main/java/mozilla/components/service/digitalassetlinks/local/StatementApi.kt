/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.local

import androidx.core.net.toUri
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.concept.fetch.Headers.Values.CONTENT_TYPE_APPLICATION_JSON
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.IncludeStatement
import mozilla.components.service.digitalassetlinks.Relation
import mozilla.components.service.digitalassetlinks.Statement
import mozilla.components.service.digitalassetlinks.StatementListFetcher
import mozilla.components.service.digitalassetlinks.StatementResult
import mozilla.components.service.digitalassetlinks.TIMEOUT
import mozilla.components.service.digitalassetlinks.ext.safeFetch
import mozilla.components.support.ktx.android.org.json.asSequence
import mozilla.components.support.ktx.kotlin.sanitizeURL
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

/**
 * Builds a list of statements by sending HTTP requests to the given website.
 */
class StatementApi(private val httpClient: Client) : StatementListFetcher {

    /**
     * Lazily list all the statements in the given [source] website.
     * If include statements are present, they will be resolved lazily.
     */
    override fun listStatements(source: AssetDescriptor.Web): Sequence<Statement> {
        val url = source.site.toUri().buildUpon()
            .path("/.well-known/assetlinks.json")
            .build()
            .toString()
        return getWebsiteStatementList(url, seenSoFar = mutableSetOf())
    }

    /**
     * Recursively download all the website statements.
     * @param assetLinksUrl URL to download.
     * @param seenSoFar URLs that have been downloaded already.
     * Used to prevent infinite loops.
     */
    private fun getWebsiteStatementList(
        assetLinksUrl: String,
        seenSoFar: MutableSet<String>,
    ): Sequence<Statement> {
        if (assetLinksUrl in seenSoFar) {
            return emptySequence()
        } else {
            seenSoFar.add(assetLinksUrl)
        }

        val request = Request(
            url = assetLinksUrl.sanitizeURL(),
            method = Request.Method.GET,
            connectTimeout = TIMEOUT,
            readTimeout = TIMEOUT,
        )
        val response = httpClient.safeFetch(request)?.let { res ->
            val contentTypes = res.headers.getAll(CONTENT_TYPE)
            if (contentTypes.any { it.contains(CONTENT_TYPE_APPLICATION_JSON, ignoreCase = true) }) {
                res
            } else {
                null
            }
        }

        val statements = response?.let { parseStatementResponse(response) }.orEmpty()
        return sequence<Statement> {
            val includeStatements = mutableListOf<IncludeStatement>()
            // Yield all statements that have already been downloaded
            statements.forEach { statement ->
                when (statement) {
                    is Statement -> yield(statement)
                    is IncludeStatement -> includeStatements.add(statement)
                }
            }
            // Recursively download include statements
            yieldAll(
                includeStatements.asSequence().flatMap {
                    getWebsiteStatementList(it.include, seenSoFar)
                },
            )
        }
    }

    /**
     * Parse the JSON [Response] returned by the website.
     */
    private fun parseStatementResponse(response: Response): List<StatementResult> {
        val responseBody = response.use { response.body.string() }
        return try {
            val responseJson = JSONArray(responseBody)
            parseStatementListJson(responseJson)
        } catch (e: JSONException) {
            emptyList()
        }
    }

    private fun parseStatementListJson(json: JSONArray): List<StatementResult> {
        return json.asSequence { i -> getJSONObject(i) }
            .flatMap { parseStatementJson(it) }
            .toList()
    }

    private fun parseStatementJson(json: JSONObject): Sequence<StatementResult> {
        val include = json.optString("include")
        if (include.isNotEmpty()) {
            return sequenceOf(IncludeStatement(include))
        }

        val relationTypes = Relation.values()
        val relations = json.getJSONArray("relation")
            .asSequence { i -> getString(i) }
            .mapNotNull { relation -> relationTypes.find { relation == it.kindAndDetail } }

        return relations.flatMap { relation ->
            val target = json.getJSONObject("target")
            val assets = when (target.getString("namespace")) {
                "web" -> sequenceOf(
                    AssetDescriptor.Web(site = target.getString("site")),
                )
                "android_app" -> {
                    val packageName = target.getString("package_name")
                    target.getJSONArray("sha256_cert_fingerprints")
                        .asSequence { i -> getString(i) }
                        .map { fingerprint -> AssetDescriptor.Android(packageName, fingerprint) }
                }
                else -> emptySequence()
            }

            assets.map { asset -> Statement(relation, asset) }
        }
    }
}

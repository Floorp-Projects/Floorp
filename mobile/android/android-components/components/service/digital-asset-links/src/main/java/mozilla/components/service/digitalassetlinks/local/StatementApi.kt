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
import mozilla.components.service.digitalassetlinks.Statement
import mozilla.components.service.digitalassetlinks.StatementLister
import mozilla.components.service.digitalassetlinks.StatementResult
import mozilla.components.service.digitalassetlinks.ext.TIMEOUT
import mozilla.components.service.digitalassetlinks.ext.safeFetch
import mozilla.components.service.digitalassetlinks.parseStatementListJson
import org.json.JSONArray
import org.json.JSONException

class StatementApi(private val httpClient: Client) : StatementLister {

    override fun listDigitalAssetLinkStatements(source: AssetDescriptor.Web): List<Statement> {
        val url = source.site.toUri().buildUpon()
            .path("/.well-known/assetlinks.json")
            .build()
            .toString()
        return getWebsiteStatementList(url)
    }

    fun getWebsiteStatementList(assetLinksUrl: String): List<Statement> {
        val request = Request(
            url = assetLinksUrl,
            method = Request.Method.GET,
            connectTimeout = TIMEOUT,
            readTimeout = TIMEOUT
        )
        val response = httpClient.safeFetch(request)?.let {
            val contentTypes = it.headers.getAll(CONTENT_TYPE)
            if (CONTENT_TYPE_APPLICATION_JSON in contentTypes) it else null
        }

        val statements = response?.let { parseStatementResponse(response) }.orEmpty()
        return statements.flatMap { statement ->
            when (statement) {
                is Statement -> listOf(statement)
                is IncludeStatement -> getWebsiteStatementList(statement.include)
            }
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
}

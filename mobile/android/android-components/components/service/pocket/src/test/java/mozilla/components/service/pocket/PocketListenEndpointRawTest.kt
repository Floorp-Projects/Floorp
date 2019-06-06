/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.concept.fetch.Headers.Names.USER_AGENT
import mozilla.components.concept.fetch.Headers.Values.CONTENT_TYPE_FORM_URLENCODED
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.service.pocket.PocketListenEndpointRaw.Companion.X_ACCESS_TOKEN
import mozilla.components.service.pocket.helpers.MockResponses
import mozilla.components.service.pocket.helpers.assertRequestParams
import mozilla.components.service.pocket.helpers.assertResponseIsClosed
import mozilla.components.service.pocket.helpers.assertSuccessfulRequestReturnsResponseBody
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import java.io.IOException
import java.net.URLEncoder

private const val EXPECTED_USER_AGENT = "Mozilla/5.0 (Expected-User-Agent 4.0) Gecko WebKit-like. Isn't this clear?"
private const val EXPECTED_ACCESS_TOKEN = "12345" // Amazing, that's the same combination on my luggage!

private val TEST_URL = "https://mozilla.org".toUri()

@RunWith(AndroidJUnit4::class)
class PocketListenEndpointRawTest {

    private lateinit var subject: PocketListenEndpointRaw
    private lateinit var client: Client
    private lateinit var urls: PocketListenURLs

    private lateinit var errorResponse: Response
    private lateinit var successResponse: Response

    @Before
    fun setUp() {
        errorResponse = MockResponses.getError()
        successResponse = MockResponses.getSuccess()

        client = mock()
        urls = mock<PocketListenURLs>().also {
            `when`(it.articleService).thenReturn(TEST_URL)
        }

        subject = PocketListenEndpointRaw(client, urls, EXPECTED_USER_AGENT, EXPECTED_ACCESS_TOKEN)
    }

    @Test
    fun `WHEN requesting listen article metadata THEN the listen article metadata url is used`() {
        val expectedUrl = "https://mozilla.org/listen-article-metadata"
        `when`(urls.articleService).thenReturn(expectedUrl.toUri())
        assertRequestParams(client, { makeRequestArticleListenMetadata() }, assertParams = { request ->
            assertEquals(expectedUrl, request.url)
        })
    }

    @Test
    fun `WHEN requesting listen article metadata THEN a POST request is used`() {
        assertRequestParams(client, { makeRequestArticleListenMetadata() }, assertParams = { request ->
            assertEquals(Request.Method.POST, request.method)
        })
    }

    @Test
    fun `WHEN requesting listen article metadata THEN the request headers are correct`() {
        assertRequestParams(client, { makeRequestArticleListenMetadata() }, assertParams = { request ->
            assertNotNull(request.headers)
            val headers = request.headers!!
            assertEquals(CONTENT_TYPE_FORM_URLENCODED, headers[CONTENT_TYPE])
            assertEquals(EXPECTED_USER_AGENT, headers[USER_AGENT])
            assertEquals(EXPECTED_ACCESS_TOKEN, headers[X_ACCESS_TOKEN])
        })
    }

    @Test
    fun `WHEN request listen article metadata THEN the request body is correct`() {
        val inputArticleID = 42L
        val inputArticleURL = "https://mozac.org/api/mozilla.components.support.ktx.android.util/"

        val expectedBodyValues = mapOf(
            "url" to URLEncoder.encode(inputArticleURL, Charsets.UTF_8.name()),
            "article_id" to inputArticleID.toString(),
            "v" to "2",
            "locale" to "en-US"
        )

        assertRequestParams(client, makeRequest = {
            subject.getArticleListenMetadata(inputArticleID, inputArticleURL)
        }, assertParams = { request ->
            assertNotNull(request.body)
            val bodyParameters = request.body!!.useStream {
                it.bufferedReader().readText().split("&")
            }

            val seenBodyParamKeys = mutableSetOf<String>()
            bodyParameters.forEach {
                val (key, value) = it.split("=")
                assertEquals("for key: $key", expectedBodyValues[key], value)

                seenBodyParamKeys.add(key)
            }

            assertEquals("Did not check all expected body params", expectedBodyValues.keys, seenBodyParamKeys)
        })
    }

    @Test
    fun `WHEN requesting listen article metadata and the client throws an IOException THEN null is returned`() {
        `when`(client.fetch(any())).thenThrow(IOException::class.java)
        assertNull(makeRequestArticleListenMetadata())
    }

    @Test
    fun `WHEN requesting listen article metadata and the response is null THEN null is returned`() {
        `when`(client.fetch(any())).thenReturn(null)
        assertNull(makeRequestArticleListenMetadata())
    }

    @Test
    fun `WHEN requesting listen article metadata and the response is not a success THEN null is returned`() {
        `when`(client.fetch(any())).thenReturn(errorResponse)
        assertNull(makeRequestArticleListenMetadata())
    }

    @Test // if invalid article parameters are passed to the server, 404 is returned.
    fun `WHEN requesting listen article metadata and the response is a 404 THEN null is returned`() {
        val response404 = MockResponses.getMockResponse(404)
        `when`(client.fetch(any())).thenReturn(response404)
        assertNull(makeRequestArticleListenMetadata())
    }

    @Test
    fun `WHEN requesting listen article metadata and the response is a success THEN the response body is returned`() {
        assertSuccessfulRequestReturnsResponseBody(client, ::makeRequestArticleListenMetadata)
    }

    @Test
    fun `WHEN requesting listen article metadata and the response is an error THEN response is closed`() {
        assertResponseIsClosed(client, errorResponse) { makeRequestArticleListenMetadata() }
    }

    @Test
    fun `WHEN requesting listen article metadata and the response is a success THEN response is closed`() {
        assertResponseIsClosed(client, successResponse) { makeRequestArticleListenMetadata() }
    }

    private fun makeRequestArticleListenMetadata(): String? {
        return subject.getArticleListenMetadata(42, "https://mozilla.org")
    }
}

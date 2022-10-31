/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.api

import android.net.Uri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.Response.Companion.SUCCESS
import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.Relation.HANDLE_ALL_URLS
import mozilla.components.service.digitalassetlinks.Relation.USE_AS_ORIGIN
import mozilla.components.service.digitalassetlinks.Statement
import mozilla.components.service.digitalassetlinks.TIMEOUT
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DigitalAssetLinksApiTest {

    private val webAsset = AssetDescriptor.Web(site = "https://mozilla.org")
    private val androidAsset = AssetDescriptor.Android(
        packageName = "com.mozilla.fenix",
        sha256CertFingerprint = "01:23:45:67:89",
    )
    private val baseRequest = Request(
        url = "https://mozilla.org",
        method = Request.Method.GET,
        connectTimeout = TIMEOUT,
        readTimeout = TIMEOUT,
    )
    private val apiKey = "X"
    private lateinit var client: Client
    private lateinit var api: DigitalAssetLinksApi

    @Before
    fun setup() {
        client = mock()
        api = DigitalAssetLinksApi(client, apiKey)

        doReturn(mockResponse("")).`when`(client).fetch(any())
    }

    @Test
    fun `reject for invalid status`() {
        val response = mockResponse("").copy(status = 400)
        doReturn(response).`when`(client).fetch(any())

        assertFalse(api.checkRelationship(webAsset, USE_AS_ORIGIN, androidAsset))
        assertEquals(emptyList<Statement>(), api.listStatements(webAsset).toList())
    }

    @Test
    fun `reject check for invalid json`() {
        doReturn(mockResponse("")).`when`(client).fetch(any())
        assertFalse(api.checkRelationship(webAsset, USE_AS_ORIGIN, webAsset))

        doReturn(mockResponse("{}")).`when`(client).fetch(any())
        assertFalse(api.checkRelationship(webAsset, USE_AS_ORIGIN, androidAsset))

        doReturn(mockResponse("[]")).`when`(client).fetch(any())
        assertFalse(api.checkRelationship(webAsset, USE_AS_ORIGIN, androidAsset))

        doReturn(mockResponse("{\"lnkd\":true}")).`when`(client).fetch(any())
        assertFalse(api.checkRelationship(webAsset, USE_AS_ORIGIN, androidAsset))
    }

    @Test
    fun `reject list for invalid json`() {
        val empty = emptyList<Statement>()

        doReturn(mockResponse("")).`when`(client).fetch(any())
        assertEquals(empty, api.listStatements(webAsset).toList())

        doReturn(mockResponse("{}")).`when`(client).fetch(any())
        assertEquals(empty, api.listStatements(webAsset).toList())

        doReturn(mockResponse("[]")).`when`(client).fetch(any())
        assertEquals(empty, api.listStatements(webAsset).toList())

        doReturn(mockResponse("{\"stmt\":[]}")).`when`(client).fetch(any())
        assertEquals(empty, api.listStatements(webAsset).toList())
    }

    @Test
    fun `return linked from json`() {
        doReturn(mockResponse("{\"linked\":true,\"maxAge\":\"3s\"}")).`when`(client).fetch(any())
        assertTrue(api.checkRelationship(webAsset, USE_AS_ORIGIN, androidAsset))

        doReturn(mockResponse("{\"linked\":false}\"maxAge\":\"3s\"}")).`when`(client).fetch(any())
        assertFalse(api.checkRelationship(webAsset, USE_AS_ORIGIN, androidAsset))
    }

    @Test
    fun `return empty list if json doesn't match expected format`() {
        val jsonPrefix = "{\"statements\":["
        val jsonSuffix = "],\"maxAge\":\"3s\"}"
        doReturn(mockResponse(jsonPrefix + jsonSuffix)).`when`(client).fetch(any())
        assertEquals(emptyList<Statement>(), api.listStatements(webAsset).toList())

        val invalidRelation = """
        {
            "source": {"web":{"site": "https://mozilla.org"}},
            "target": {"web":{"site": "https://mozilla.org"}},
            "relation": "not-a-relation"
        }
        """
        doReturn(mockResponse(jsonPrefix + invalidRelation + jsonSuffix)).`when`(client).fetch(any())
        assertEquals(emptyList<Statement>(), api.listStatements(webAsset).toList())

        val invalidTarget = """
        {
            "source": {"web":{"site": "https://mozilla.org"}},
            "target": {},
            "relation": "delegate_permission/common.use_as_origin"
        }
        """
        doReturn(mockResponse(jsonPrefix + invalidTarget + jsonSuffix)).`when`(client).fetch(any())
        assertEquals(emptyList<Statement>(), api.listStatements(webAsset).toList())
    }

    @Test
    fun `parses json statement list with web target`() {
        val webStatement = """
        {"statements": [{
            "source": {"web":{"site": "https://mozilla.org"}},
            "target": {"web":{"site": "https://mozilla.org"}},
            "relation": "delegate_permission/common.use_as_origin"
        }], "maxAge": "59s"}
        """
        doReturn(mockResponse(webStatement)).`when`(client).fetch(any())
        assertEquals(
            listOf(
                Statement(
                    relation = USE_AS_ORIGIN,
                    target = webAsset,
                ),
            ),
            api.listStatements(webAsset).toList(),
        )
    }

    @Test
    fun `parses json statement list with android target`() {
        val androidStatement = """
        {"statements": [{
            "source": {"web":{"site": "https://mozilla.org"}},
            "target": {"androidApp":{ 
                "packageName": "com.mozilla.fenix", 
                "certificate": {"sha256Fingerprint": "01:23:45:67:89"}
            }},
            "relation": "delegate_permission/common.handle_all_urls"
        }], "maxAge": "2m"}
        """
        doReturn(mockResponse(androidStatement)).`when`(client).fetch(any())
        assertEquals(
            listOf(
                Statement(
                    relation = HANDLE_ALL_URLS,
                    target = androidAsset,
                ),
            ),
            api.listStatements(webAsset).toList(),
        )
    }

    @Test
    fun `passes data in get check request URL for android target`() {
        api.checkRelationship(webAsset, USE_AS_ORIGIN, androidAsset)
        verify(client).fetch(
            baseRequest.copy(
                url = "https://digitalassetlinks.googleapis.com/v1/assetlinks:check?" +
                    "prettyPrint=false&key=X&relation=delegate_permission%2Fcommon.use_as_origin&" +
                    "source.web.site=${Uri.encode("https://mozilla.org")}&" +
                    "target.androidApp.packageName=com.mozilla.fenix&" +
                    "target.androidApp.certificate.sha256Fingerprint=${Uri.encode("01:23:45:67:89")}",
            ),
        )
    }

    @Test
    fun `passes data in get check request URL for web target`() {
        api.checkRelationship(webAsset, HANDLE_ALL_URLS, webAsset)
        verify(client).fetch(
            baseRequest.copy(
                url = "https://digitalassetlinks.googleapis.com/v1/assetlinks:check?" +
                    "prettyPrint=false&key=X&relation=delegate_permission%2Fcommon.handle_all_urls&" +
                    "source.web.site=${Uri.encode("https://mozilla.org")}&" +
                    "target.web.site=${Uri.encode("https://mozilla.org")}",
            ),
        )
    }

    @Test
    fun `passes data in get list request URL`() {
        api.listStatements(webAsset)
        verify(client).fetch(
            baseRequest.copy(
                url = "https://digitalassetlinks.googleapis.com/v1/statements:list?" +
                    "prettyPrint=false&key=X&source.web.site=${Uri.encode("https://mozilla.org")}",
            ),
        )
    }

    private fun mockResponse(data: String) = Response(
        url = "",
        status = SUCCESS,
        headers = MutableHeaders(),
        body = mockBody(data),
    )

    private fun mockBody(data: String): Response.Body {
        val mockBody: Response.Body = mock()
        doReturn(data).`when`(mockBody).string()
        return mockBody
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.local

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.concept.fetch.Headers.Values.CONTENT_TYPE_APPLICATION_JSON
import mozilla.components.concept.fetch.Headers.Values.CONTENT_TYPE_FORM_URLENCODED
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.service.digitalassetlinks.AssetDescriptor
import mozilla.components.service.digitalassetlinks.Relation
import mozilla.components.service.digitalassetlinks.Statement
import mozilla.components.service.digitalassetlinks.StatementListFetcher
import mozilla.components.service.digitalassetlinks.TIMEOUT
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations
import java.io.ByteArrayInputStream
import java.io.IOException

@RunWith(AndroidJUnit4::class)
class StatementApiTest {

    @Mock private lateinit var httpClient: Client
    private lateinit var listFetcher: StatementListFetcher
    private val jsonHeaders = MutableHeaders(
        CONTENT_TYPE to CONTENT_TYPE_APPLICATION_JSON,
    )

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        listFetcher = StatementApi(httpClient)
    }

    @Test
    fun `return empty list if request fails`() {
        `when`(
            httpClient.fetch(
                Request(
                    url = "https://mozilla.org/.well-known/assetlinks.json",
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT,
                ),
            ),
        ).thenThrow(IOException::class.java)

        val source = AssetDescriptor.Web("https://mozilla.org")
        assertEquals(emptyList<Statement>(), listFetcher.listStatements(source).toList())
    }

    @Test
    fun `return empty list if response does not have status 200`() {
        val response = Response(
            url = "https://firefox.com/.well-known/assetlinks.json",
            status = 201,
            headers = jsonHeaders,
            body = mock(),
        )
        `when`(
            httpClient.fetch(
                Request(
                    url = "https://firefox.com/.well-known/assetlinks.json",
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT,
                ),
            ),
        ).thenReturn(response)

        val source = AssetDescriptor.Web("https://firefox.com")
        assertEquals(emptyList<Statement>(), listFetcher.listStatements(source).toList())
    }

    @Test
    fun `return empty list if response does not have JSON content type`() {
        val response = Response(
            url = "https://firefox.com/.well-known/assetlinks.json",
            status = 200,
            headers = MutableHeaders(
                CONTENT_TYPE to CONTENT_TYPE_FORM_URLENCODED,
            ),
            body = mock(),
        )

        `when`(
            httpClient.fetch(
                Request(
                    url = "https://firefox.com/.well-known/assetlinks.json",
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT,
                ),
            ),
        ).thenReturn(response)

        val source = AssetDescriptor.Web("https://firefox.com")
        assertEquals(emptyList<Statement>(), listFetcher.listStatements(source).toList())
    }

    @Test
    fun `return empty list if response is not valid JSON`() {
        val response = Response(
            url = "http://firefox.com/.well-known/assetlinks.json",
            status = 200,
            headers = jsonHeaders,
            body = stringBody("not-json"),
        )

        `when`(
            httpClient.fetch(
                Request(
                    url = "http://firefox.com/.well-known/assetlinks.json",
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT,
                ),
            ),
        ).thenReturn(response)

        val source = AssetDescriptor.Web("http://firefox.com")
        assertEquals(emptyList<Statement>(), listFetcher.listStatements(source).toList())
    }

    @Test
    fun `return empty list if response is an empty JSON array`() {
        val response = Response(
            url = "http://firefox.com/.well-known/assetlinks.json",
            status = 200,
            headers = jsonHeaders,
            body = stringBody("[]"),
        )

        `when`(
            httpClient.fetch(
                Request(
                    url = "http://firefox.com/.well-known/assetlinks.json",
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT,
                ),
            ),
        ).thenReturn(response)

        val source = AssetDescriptor.Web("http://firefox.com")
        assertEquals(emptyList<Statement>(), listFetcher.listStatements(source).toList())
    }

    @Test
    fun `parses example asset links file`() {
        val response = Response(
            url = "http://firefox.com/.well-known/assetlinks.json",
            status = 200,
            headers = jsonHeaders,
            body = stringBody(
                """
            [{
                "relation": [
                    "delegate_permission/common.handle_all_urls",
                    "delegate_permission/common.use_as_origin"
                ],
                "target": {
                    "namespace": "web",
                    "site": "https://www.google.com"
                }
            },{
                "relation": ["delegate_permission/common.handle_all_urls"],
                "target": {
                    "namespace": "android_app",
                    "package_name": "org.digitalassetlinks.sampleapp",
                    "sha256_cert_fingerprints": [
                        "10:39:38:EE:45:37:E5:9E:8E:E7:92:F6:54:50:4F:B8:34:6F:C6:B3:46:D0:BB:C4:41:5F:C3:39:FC:FC:8E:C1"
                    ]
                }
            },{
                "relation": ["delegate_permission/common.handle_all_urls"],
                "target": {
                    "namespace": "android_app",
                    "package_name": "org.digitalassetlinks.sampleapp2",
                    "sha256_cert_fingerprints": ["AA", "BB"]
                }
            }]
            """,
            ),
        )
        `when`(
            httpClient.fetch(
                Request(
                    url = "http://firefox.com/.well-known/assetlinks.json",
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT,
                ),
            ),
        ).thenReturn(response)

        val source = AssetDescriptor.Web("http://firefox.com")
        assertEquals(
            listOf(
                Statement(
                    relation = Relation.HANDLE_ALL_URLS,
                    target = AssetDescriptor.Web("https://www.google.com"),
                ),
                Statement(
                    relation = Relation.USE_AS_ORIGIN,
                    target = AssetDescriptor.Web("https://www.google.com"),
                ),
                Statement(
                    relation = Relation.HANDLE_ALL_URLS,
                    target = AssetDescriptor.Android(
                        packageName = "org.digitalassetlinks.sampleapp",
                        sha256CertFingerprint = "10:39:38:EE:45:37:E5:9E:8E:E7:92:F6:54:50:4F:B8:34:6F:C6:B3:46:D0:BB:C4:41:5F:C3:39:FC:FC:8E:C1",
                    ),
                ),
                Statement(
                    relation = Relation.HANDLE_ALL_URLS,
                    target = AssetDescriptor.Android(
                        packageName = "org.digitalassetlinks.sampleapp2",
                        sha256CertFingerprint = "AA",
                    ),
                ),
                Statement(
                    relation = Relation.HANDLE_ALL_URLS,
                    target = AssetDescriptor.Android(
                        packageName = "org.digitalassetlinks.sampleapp2",
                        sha256CertFingerprint = "BB",
                    ),
                ),
            ),
            listFetcher.listStatements(source).toList(),
        )
    }

    @Test
    fun `resolves include statements`() {
        `when`(
            httpClient.fetch(
                Request(
                    url = "http://firefox.com/.well-known/assetlinks.json",
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT,
                ),
            ),
        ).thenReturn(
            Response(
                url = "http://firefox.com/.well-known/assetlinks.json",
                status = 200,
                headers = jsonHeaders,
                body = stringBody(
                    """
            [{
                "relation": ["delegate_permission/common.use_as_origin"],
                "target": {
                    "namespace": "web",
                    "site": "https://www.google.com"
                }
            },{
                "include": "https://example.com/includedstatements.json"
            }]
            """,
                ),
            ),
        )
        `when`(
            httpClient.fetch(
                Request(
                    url = "https://example.com/includedstatements.json",
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT,
                ),
            ),
        ).thenReturn(
            Response(
                url = "https://example.com/includedstatements.json",
                status = 200,
                headers = jsonHeaders,
                body = stringBody(
                    """
            [{
                "relation": ["delegate_permission/common.use_as_origin"],
                "target": {
                    "namespace": "web",
                    "site": "https://www.example.com"
                }
            }]
            """,
                ),
            ),
        )

        val source = AssetDescriptor.Web("http://firefox.com")
        assertEquals(
            listOf(
                Statement(
                    relation = Relation.USE_AS_ORIGIN,
                    target = AssetDescriptor.Web("https://www.google.com"),
                ),
                Statement(
                    relation = Relation.USE_AS_ORIGIN,
                    target = AssetDescriptor.Web("https://www.example.com"),
                ),
            ),
            listFetcher.listStatements(source).toList(),
        )
    }

    @Test
    fun `no infinite loops`() {
        `when`(
            httpClient.fetch(
                Request(
                    url = "http://firefox.com/.well-known/assetlinks.json",
                    connectTimeout = TIMEOUT,
                    readTimeout = TIMEOUT,
                ),
            ),
        ).thenReturn(
            Response(
                url = "http://firefox.com/.well-known/assetlinks.json",
                status = 200,
                headers = jsonHeaders,
                body = stringBody(
                    """
            [{
                "relation": ["delegate_permission/common.use_as_origin"],
                "target": {
                    "namespace": "web",
                    "site": "https://example.com"
                }
            },{
                "include": "http://firefox.com/.well-known/assetlinks.json"
            }]
            """,
                ),
            ),
        )

        val source = AssetDescriptor.Web("http://firefox.com")
        assertEquals(
            listOf(
                Statement(
                    relation = Relation.USE_AS_ORIGIN,
                    target = AssetDescriptor.Web("https://example.com"),
                ),
            ),
            listFetcher.listStatements(source).toList(),
        )
    }

    private fun stringBody(data: String) = Response.Body(ByteArrayInputStream(data.toByteArray()))
}

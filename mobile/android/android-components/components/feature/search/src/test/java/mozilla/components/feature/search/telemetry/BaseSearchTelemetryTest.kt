/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import java.io.File

@RunWith(AndroidJUnit4::class)
class BaseSearchTelemetryTest {

    private lateinit var baseTelemetry: BaseSearchTelemetry
    private lateinit var handler: BaseSearchTelemetry.SearchTelemetryMessageHandler

    @Mock
    private lateinit var mockRepo: SerpTelemetryRepository

    private val mockReadJson: () -> JSONObject = mock()
    private val mockRootStorageDirectory: File = mock()

    private fun createMockProviderList(): List<SearchProviderModel> = listOf(
        SearchProviderModel(
            schema = 1698656464939,
            taggedCodes = listOf("monline_7_dg"),
            telemetryId = "baidu",
            organicCodes = emptyList(),
            codeParamName = "tn",
            queryParamNames = listOf("wd"),
            searchPageRegexp = "^https://(?:m|www)\\\\.baidu\\\\.com/(?:s|baidu)",
            followOnParamNames = listOf("oq"),
            extraAdServersRegexps = listOf("^https?://www\\\\.baidu\\\\.com/baidu\\\\.php?"),
            expectedOrganicCodes = emptyList(),
        ),
    )

    private val rawJson = """
        {
  "data": [
    {
      "schema": 1698656464939,
      "taggedCodes": [
        "monline_7_dg"
      ],
      "telemetryId": "baidu",
      "organicCodes": [],
      "codeParamName": "tn",
      "queryParamNames": [
        "wd"
      ],
      "searchPageRegexp": "^https://(?:m|www)\\.baidu\\.com/(?:s|baidu)",
      "followOnParamNames": [
        "oq"
      ],
      "extraAdServersRegexps": [
        "^https?://www\\.baidu\\.com/baidu\\.php?"
      ],
      "id": "19c434a3-d173-4871-9743-290ac92a3f6a",
      "last_modified": 1698666532326
    }],
  "timestamp": 16
}
    """.trimIndent()

    @Before
    fun setup() {
        baseTelemetry = spy(
            object : BaseSearchTelemetry() {
                override suspend fun install(
                    engine: Engine,
                    store: BrowserStore,
                    providerList: List<SearchProviderModel>,
                ) {
                    // mock, do nothing
                }

                override fun processMessage(message: JSONObject) {
                    // mock, do nothing
                }
            },
        )
        handler = baseTelemetry.SearchTelemetryMessageHandler()
        mockRepo = spy(SerpTelemetryRepository(mockRootStorageDirectory, mockReadJson, "test"))
    }

    @Test
    fun `GIVEN an engine WHEN installWebExtension is called THEN the provided extension is installed in engine`() {
        val engine: Engine = mock()
        val store: BrowserStore = mock()
        val id = "id"
        val resourceUrl = "resourceUrl"
        val messageId = "messageId"
        val extensionInfo = ExtensionInfo(id, resourceUrl, messageId)

        baseTelemetry.installWebExtension(engine, store, extensionInfo)

        verify(engine).installBuiltInWebExtension(
            id = eq(id),
            url = eq(resourceUrl),
            onSuccess = any(),
            onError = any(),
        )
    }

    @Test
    fun `GIVEN a search provider does not exist for the url WHEN getProviderForUrl is called THEN return null`() {
        val url = "https://www.mozilla.com/search?q=firefox"
        baseTelemetry.providerList = createMockProviderList()

        assertEquals(null, baseTelemetry.getProviderForUrl(url))
    }

    @Test(expected = IllegalStateException::class)
    fun `GIVEN an extension message WHEN that cannot be processed THEN throw IllegalStateException`() {
        val message = "message"

        handler.onMessage(message, mock())
    }

    @Test
    fun `GIVEN an extension message WHEN received THEN pass it to processMessage`() {
        val message = JSONObject()

        handler.onMessage(message, mock())

        verify(baseTelemetry).processMessage(message)
    }

    @Test
    fun `GIVEN empty cacheResponse WHEN initializeProviderList is called THEN  update providerList`(): Unit =
        runBlocking {
            val localResponse = JSONObject(rawJson)
            val cacheResponse: Pair<ULong, List<SearchProviderModel>> = Pair(0u, emptyList())

            `when`(mockRepo.loadProvidersFromCache()).thenReturn(cacheResponse)
            doAnswer {
                localResponse
            }.`when`(mockReadJson)()

            `when`(mockRepo.parseLocalPreinstalledData(localResponse)).thenReturn(createMockProviderList())
            doReturn(Unit).`when`(mockRepo).fetchRemoteResponse(any())

            baseTelemetry.setProviderList(mockRepo.updateProviderList())

            assertEquals(baseTelemetry.providerList.toString(), createMockProviderList().toString())
        }

    @Test
    fun `GIVEN non-empty cacheResponse WHEN initializeProviderList is called THEN update providerList`(): Unit =
        runBlocking {
            val localResponse = JSONObject(rawJson)
            val cacheResponse: Pair<ULong, List<SearchProviderModel>> = Pair(123u, createMockProviderList())

            `when`(mockRepo.loadProvidersFromCache()).thenReturn(cacheResponse)
            doAnswer {
                localResponse
            }.`when`(mockReadJson)()
            doReturn(Unit).`when`(mockRepo).fetchRemoteResponse(any())

            baseTelemetry.setProviderList(mockRepo.updateProviderList())

            assertEquals(baseTelemetry.providerList.toString(), createMockProviderList().toString())
        }

    fun getProviderForUrl(url: String): SearchProviderModel? {
        return createMockProviderList().find { provider ->
            provider.searchPageRegexp.pattern.toRegex().containsMatchIn(url)
        }
    }
}

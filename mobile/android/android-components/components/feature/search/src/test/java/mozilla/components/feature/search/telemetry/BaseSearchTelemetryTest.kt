/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import androidx.test.ext.junit.runners.AndroidJUnit4
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
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.File

@RunWith(AndroidJUnit4::class)
class BaseSearchTelemetryTest {

    private lateinit var baseTelemetry: BaseSearchTelemetry
    private lateinit var handler: BaseSearchTelemetry.SearchTelemetryMessageHandler

    @Mock
    private lateinit var mockParser: SerpTelemetryFetcher

    private fun createMockProviderList(): List<SearchProviderModel> = listOf(
        SearchProviderModel(
            schema = 1671479978127,
            taggedCodes = listOf("abc", "xyz", "def"),
            telemetryId = "google",
            organicCodes = emptyList(),
            codeParamName = "tt",
            queryParamNames = listOf("q"),
            searchPageRegexp = "^https://www\\.example\\.org/",
            extraAdServersRegexps = listOf("^https://www\\.google\\.(?:.+)/search"),
            expectedOrganicCodes = emptyList(),
        ),
    )

    @Before
    fun setup() {
        baseTelemetry = spy(
            object : BaseSearchTelemetry() {
                override suspend fun install(engine: Engine, store: BrowserStore, rootStorageDirectory: File) {
                    // mock, do nothing
                }

                override fun processMessage(message: JSONObject) {
                    // mock, do nothing
                }
            },
        )
        handler = baseTelemetry.SearchTelemetryMessageHandler()
        mockParser = SerpTelemetryFetcher(mock(), "test")
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

    fun getProviderForUrl(url: String): SearchProviderModel? {
        return createMockProviderList().find { provider ->
            provider.searchPageRegexp.pattern.toRegex().containsMatchIn(url)
        }
    }
}

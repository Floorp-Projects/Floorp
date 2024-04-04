package mozilla.components.support.remotesettings

import kotlinx.coroutines.runBlocking
import mozilla.appservices.remotesettings.RemoteSettings
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import mozilla.appservices.remotesettings.RemoteSettingsResponse
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner
import java.io.File
import java.io.FileReader
import java.io.IOException

@RunWith(RobolectricTestRunner::class)
class RemoteSettingsClientTest {

    private lateinit var mockRemoteSettings: RemoteSettings
    private lateinit var mockFile: File
    private lateinit var mockFileReader: FileReader
    private lateinit var mockJsonObject: JSONObject
    private lateinit var remoteSettingsClient: RemoteSettingsClient

    @get:Rule
    val tempFolder = TemporaryFolder()

    @Before
    fun setUp() {
        mockFile = mock()
        mockFileReader = mock()
        mockJsonObject = mock()
        mockRemoteSettings = mock()
        remoteSettingsClient = RemoteSettingsClient(File(""), "https://firefox.settings.services.mozilla.com", "", "")
    }

    @Test
    fun `GIVEN non-empty records WHEN write is called THEN the result is a success`() {
        val tempFile = File.createTempFile("test", ".json")
        val records = listOf(
            RemoteSettingsRecord("1", 123u, false, null, JSONObject()),
            RemoteSettingsRecord("2", 456u, true, null, JSONObject()),
        )
        val response = RemoteSettingsResponse(records, 125614567U)
        val result = runBlocking { RemoteSettingsClient(tempFile, collectionName = "test").write(response) }

        require(result is RemoteSettingsResult.Success) { "Result should be Success" }
        assertEquals(response, result.response)

        tempFile.delete()
    }

    @Test
    fun `GIVEN non-empty records WHEN write is called THEN read text from file is same as write text to file`() {
        val path = "test.com/test/test"
        tempFolder.newFolder("test.com", "test")
        val file = tempFolder.newFile(path)
        val client = RemoteSettingsClient(tempFolder.root, "https://test.com", "test", "test")
        client.file = file
        val jsonObject = JSONObject(
            mapOf(
                "schema" to 1698656464939,
                "taggedCodes" to JSONArray(
                    listOf(
                        "monline_dg",
                        "monline_3_dg",
                        "monline_4_dg",
                        "monline_7_dg",
                    ),
                ),
                "telemetryId" to "baidu",
                "organicCodes" to JSONArray(emptyList<String>()),
                "codeParamName" to "tn",
                "queryParamName" to "wd",
                "queryParamNames" to JSONArray(
                    listOf(
                        "wd",
                        "word",
                    ),
                ),
                "searchPageRegexp" to "^https://(?:m|www)\\.baidu\\.com/(?:s|baidu)",
                "followOnParamNames" to JSONArray(
                    listOf(
                        "oq",
                    ),
                ),
                "extraAdServersRegexps" to JSONArray(
                    listOf(
                        "^https?://www\\.baidu\\.com/baidu\\.php?",
                    ),
                ),
                "id" to "19c434a3-d173-4871-9743-290ac92a3f6a",
                "last_modified" to 1698666532326,
            ),
        )
        val records = listOf(
            RemoteSettingsRecord("1", 123u, false, null, jsonObject),
        )
        val response = RemoteSettingsResponse(records, 125614567U)

        runBlocking {
            client.write(response)
            val result = client.read()

            assertEquals(response.records.first().id, (result as RemoteSettingsResult.Success).response.records.first().id)
            assertEquals(response.records.first().attachment, result.response.records.first().attachment)
            assertEquals(response.records.first().fields.getJSONArray("followOnParamNames"), result.response.records.first().fields.getJSONArray("followOnParamNames"))
        }
    }

    @Test
    fun `GIVEN non-empty records WHEN read is called THEN result content has top-level fields and arbitrarily nested fields`() {
        val path = "test.com/test/test"
        tempFolder.newFolder("test.com", "test")
        val file = tempFolder.newFile(path)
        val client = RemoteSettingsClient(tempFolder.root, "https://test.com", "test", "test")
        file.writeText(
            """
{
        "records": [
            {
                "id": "1",
                "lastModified": 123,
                "deleted": false,
                "attachment": null,
                "fields": "{
                \"codeParamName\":\"tt\",
                \"components\":[
                {\"included\":{\"children\":[{\"countChildren\":true,\"selector\":\".product-ads-carousel__item\"}], \"parent\":{\"selector\":\".product-ads-carousel\"}, \"related\":{\"selector\":\".snippet__control\"}},\"type\":\"ad_carousel\"},
                {\"included\":{\"children\":[{\"selector\":\".result__extra-content .deep-links--descriptions\",\"type\":\"ad_sitelink\"}], \"parent\":{\"selector\":\".ad-result\"}},\"type\":\"ad_link\"},
                {\"included\":{\"children\":[{\"selector\":\".search-form__input, .search-form__submit\"}],\"parent\":{\"selector\":\"form.search-form\"},\"related\":{\"selector\":\".search-form__suggestions\"}},\"topDown\":true,\"type\":\"incontent_searchbox\"},
                {\"default\":true,\"type\":\"ad_link\"}
                ],
                \"expectedOrganicCodes\":[],
                \"extraAdServersRegexps\":[\"^https:\\/\\/www\\\\.bing\\\\.com\\/acli?c?k\"],\"filter_expression\":\"env.version|versionCompare(\\\"110.0a1\\\")>=0\",
                \"organicCodes\":[],
                \"queryParamName\":\"q\",
                \"queryParamNames\":[\"q\"],
                \"schema\":1698656463945,
                \"searchPageRegexp\":\"^https:\\/\\/www\\\\.ecosia\\\\.org\\/\",\"shoppingTab\":{\"regexp\":\"\\/shopping?\",\"selector\":\"nav li[data-test-id='search-navigation-item-shopping'] a\"},
                \"taggedCodes\":[\"mzl\",\"813cf1dd\",\"16eeffc4\"],
                \"telemetryId\":\"ecosia\"
                }"
                }
        ],
        "lastModified": 123
}
"""
                .trimIndent(),
        )

        runBlocking {
            val result = client.read()
            assertEquals("1", (result as RemoteSettingsResult.Success).response.records.first().id)
            assertEquals("ad_carousel", (result.response.records.first().fields.getJSONArray("components").get(0) as JSONObject).getString("type"))
        }
    }

    @Test
    fun `GIVEN non-empty records WHEN fetchAndWrite is called THEN the result is a success`() {
        val client = mock<RemoteSettingsClient>()
        val records = listOf(RemoteSettingsRecord("1", 123u, false, null, JSONObject()))
        val response = RemoteSettingsResponse(records, 125614567U)
        val fetchResult = RemoteSettingsResult.Success(response)
        val writeResult = RemoteSettingsResult.Success(response)

        runBlocking {
            `when`(client.fetch()).thenReturn(fetchResult)
            `when`(client.write(response)).thenReturn(writeResult)

            val result = client.fetchAndWrite()

            assertEquals(writeResult, result)
        }
    }

    @Test
    fun `GIVEN fetch failure WHEN fetchAndWrite is called THEN the result is a failure`() {
        val client = mock<RemoteSettingsClient>()
        val fetchResult = RemoteSettingsResult.NetworkFailure(IOException("Network error"))

        runBlocking {
            `when`(client.fetch()).thenReturn(fetchResult)

            val result = client.fetchAndWrite()

            assertEquals(fetchResult, result)
        }
    }

    @Test
    fun `GIVEN read success WHEN readOrFetch is called THEN the result is a success`() {
        val client = mock<RemoteSettingsClient>()
        val records = emptyList<RemoteSettingsRecord>()
        val response = RemoteSettingsResponse(records, 125614567U)
        val readResult = RemoteSettingsResult.Success(response)

        runBlocking {
            `when`(client.read()).thenReturn(readResult)

            val result = client.readOrFetch()

            assertEquals(readResult, result)
        }
    }

    @Test
    fun `GIVEN read failure, fetch success WHEN readOrFetch is called THEN the result is a success`() {
        val client = mock<RemoteSettingsClient>()
        val records = emptyList<RemoteSettingsRecord>()
        val response = RemoteSettingsResponse(records, 125614567U)
        val readResult = RemoteSettingsResult.DiskFailure(IOException("Disk error"))
        val fetchResult = RemoteSettingsResult.Success(response)

        runBlocking {
            `when`(client.read()).thenReturn(readResult)
            `when`(client.fetch()).thenReturn(fetchResult)

            val result = client.readOrFetch()

            assertEquals(fetchResult, result)
        }
    }
}

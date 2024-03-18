/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import kotlinx.coroutines.runBlocking
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import mozilla.appservices.remotesettings.RemoteSettingsResponse
import mozilla.components.support.remotesettings.RemoteSettingsClient
import mozilla.components.support.remotesettings.RemoteSettingsResult
import mozilla.components.support.test.mock
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SerpTelemetryRepositoryTest {
    @Mock
    private lateinit var mockRemoteSettingsClient: RemoteSettingsClient

    private lateinit var serpTelemetryRepository: SerpTelemetryRepository

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        serpTelemetryRepository = SerpTelemetryRepository(
            rootStorageDirectory = mock(),
            readJson = mock(),
            collectionName = "",
            serverUrl = "https://test.server",
            bucketName = "",
        )

        serpTelemetryRepository.remoteSettingsClient = mockRemoteSettingsClient
    }

    @Test
    fun `GIVEN non-empty response WHEN writeToCache is called THEN the result is a success`() = runBlocking {
        val records = listOf(
            RemoteSettingsRecord("1", 123u, false, null, JSONObject()),
            RemoteSettingsRecord("2", 456u, true, null, JSONObject()),
        )
        val response = RemoteSettingsResponse(records, 125614567U)

        `when`(mockRemoteSettingsClient.write(response))
            .thenReturn(RemoteSettingsResult.Success(response))

        val result = serpTelemetryRepository.writeToCache(response)

        assertTrue(result is RemoteSettingsResult.Success)
        assertEquals(response, (result as RemoteSettingsResult.Success).response)
    }

    @Test
    fun `GIVEN non-empty response WHEN fetchRemoteResponse is called THEN the result is equal to the response`() = runBlocking {
        val records = listOf(
            RemoteSettingsRecord("1", 123u, false, null, JSONObject()),
            RemoteSettingsRecord("2", 456u, true, null, JSONObject()),
        )
        val response = RemoteSettingsResponse(records, 125614567U)
        `when`(mockRemoteSettingsClient.fetch())
            .thenReturn(RemoteSettingsResult.Success(response))

        val result = serpTelemetryRepository.fetchRemoteResponse()

        assertEquals(response, result)
    }

    @Test
    fun `GIVEN non-empty response WHEN loadProvidersFromCache is called THEN the result is equal to the response`() = runBlocking {
        val records = listOf(
            RemoteSettingsRecord("1", 123u, false, null, JSONObject()),
            RemoteSettingsRecord("2", 456u, true, null, JSONObject()),
        )
        val response = RemoteSettingsResponse(records, 125614567U)
        `when`(mockRemoteSettingsClient.read())
            .thenReturn(RemoteSettingsResult.Success(response))

        val result = serpTelemetryRepository.loadProvidersFromCache()

        assertEquals(response.lastModified, result.first)
        assertEquals(response.records.mapNotNull { it.fields.toSearchProviderModel() }, result.second)
    }
}

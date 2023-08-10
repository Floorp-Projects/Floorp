package mozilla.components.support.remotesettings

import kotlinx.coroutines.runBlocking
import mozilla.appservices.remotesettings.RemoteSettings
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
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

    @Before
    fun setUp() {
        mockFile = mock()
        mockFileReader = mock()
        mockJsonObject = mock()
        mockRemoteSettings = mock()
        remoteSettingsClient = RemoteSettingsClient(File(""), "https://firefox.settings.services.mozilla.com", "", "")
    }

    @Test
    fun `GIVEN empty records WHEN write is called THEN the result is disk failure`() {
        val records = emptyList<RemoteSettingsRecord>()

        val result = runBlocking { remoteSettingsClient.write(records) }

        assertTrue(result is RemoteSettingsResult.DiskFailure)
    }

    fun `GIVEN non-empty records WHEN write is called THEN the result is a success`() {
        val tempFile = File.createTempFile("test", ".json")
        val records = listOf(
            RemoteSettingsRecord("1", 123u, false, null, JSONObject()),
            RemoteSettingsRecord("2", 456u, true, null, JSONObject()),
        )

        val result = runBlocking { RemoteSettingsClient(tempFile).write(records) }

        require(result is RemoteSettingsResult.Success) { "Result should be Success" }
        assertEquals(records, result.records)

        tempFile.delete()
    }

    @Test
    fun `GIVEN non-empty records WHEN fetchAndWrite is called THEN the result is a success`() {
        val client = mock<RemoteSettingsClient>()
        val records = listOf(RemoteSettingsRecord("1", 123u, false, null, JSONObject()))
        val fetchResult = RemoteSettingsResult.Success(records)
        val writeResult = RemoteSettingsResult.Success(records)

        runBlocking {
            `when`(client.fetch()).thenReturn(fetchResult)
            `when`(client.write(records)).thenReturn(writeResult)

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
        val readResult = RemoteSettingsResult.Success(emptyList())

        runBlocking {
            `when`(client.read()).thenReturn(readResult)

            val result = client.readOrFetch()

            assertEquals(readResult, result)
        }
    }

    @Test
    fun `GIVEN read failure, fetch success WHEN readOrFetch is called THEN the result is a success`() {
        val client = mock<RemoteSettingsClient>()
        val readResult = RemoteSettingsResult.DiskFailure(IOException("Disk error"))
        val fetchResult = RemoteSettingsResult.Success(emptyList())

        runBlocking {
            `when`(client.read()).thenReturn(readResult)
            `when`(client.fetch()).thenReturn(fetchResult)

            val result = client.readOrFetch()

            assertEquals(fetchResult, result)
        }
    }
}

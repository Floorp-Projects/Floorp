/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.ping

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.BuildConfig
import mozilla.components.service.glean.private.PingType
import mozilla.components.service.glean.storages.MockStorageEngine
import mozilla.components.service.glean.storages.StorageEngineManager
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.time.OffsetDateTime
import java.time.format.DateTimeFormatter

@RunWith(RobolectricTestRunner::class)
class PingMakerTest {
    private val mockApplicationContext = mock(Context::class.java)

    private val customPing = PingType(
        name = "test",
        includeClientId = true
    )

    // This test requires us to test against the minSdk of 21 in order to make sure that a date
    // related issue is not regressed. We do this using @Config(sdk = [21, 28]) annotation which
    // accepts an array of sdk versions to test against. Since loading the sdk versions is time
    // consuming, we only test the minSdk to reduce test overhead of glean.
    //
    // If the minSdk gets changed to >= 24, this can be removed, and DateUtils.getISOTimeString()
    // can be updated to remove the workaround.
    @Test
    @Config(sdk = [ 21, 28 ])
    fun `"ping_info" must contain a non-empty start_time and end_time`() {
        val maker = PingMaker(
            StorageEngineManager(
                storageEngines = mapOf(
                    "engine2" to MockStorageEngine(JSONObject(mapOf("a.b" to "foo")))
                ),
                applicationContext = mockApplicationContext
            ),
            mockApplicationContext
        )

        // Gather the data. We expect an empty ping with the "ping_info" information
        val data = maker.collect(customPing).orEmpty()
        assertTrue("We expect a non-empty JSON blob", "{}" != data)

        // Parse the data so that we can easily check the other fields
        val jsonData = JSONObject(data)
        val pingInfo = jsonData["ping_info"] as JSONObject
        assertNotNull(pingInfo)

        // "start_time" and "end_time" must be valid ISO8601 dates. DateTimeFormatter would
        // throw otherwise.
        DateTimeFormatter.ISO_OFFSET_DATE_TIME.parse(pingInfo.getString("start_time"))
        DateTimeFormatter.ISO_OFFSET_DATE_TIME.parse(pingInfo.getString("end_time"))
        OffsetDateTime.parse(pingInfo.getString("start_time"))
        assertTrue(OffsetDateTime.parse(pingInfo.getString("start_time"))
            <= OffsetDateTime.parse(pingInfo.getString("end_time")))
    }

    @Test
    fun `getPingInfo() must report all the required fields`() {
        val maker = PingMaker(
            StorageEngineManager(
                storageEngines = mapOf(
                    "engine2" to MockStorageEngine(JSONArray(listOf("a", "b", "c")))
                ),
                applicationContext = mockApplicationContext
            ),
            mockApplicationContext
        )

        // Gather the data. We expect an empty ping with the "ping_info" information
        val data = maker.collect(customPing).orEmpty()
        assertTrue("We expect a non-empty JSON blob", "{}" != data)

        // Parse the data so that we can easily check the other fields
        val jsonData = JSONObject(data)
        val pingInfo = jsonData["ping_info"] as JSONObject

        assertEquals("test", pingInfo.getString("ping_type"))
        assertTrue(pingInfo.has("start_time"))
        assertTrue(pingInfo.has("end_time"))
        assertTrue(pingInfo.has("seq"))
    }

    @Test
    fun `getClientInfo() must report all the available data`() {
        val maker = PingMaker(
            StorageEngineManager(
                storageEngines = mapOf(
                    "engine2" to MockStorageEngine(JSONArray(listOf("a", "b", "c")))
                ),
                applicationContext = ApplicationProvider.getApplicationContext()
            ),
            ApplicationProvider.getApplicationContext()
        )

        // Gather the data. We expect an empty ping with the "ping_info" information
        val data = maker.collect(customPing).orEmpty()
        assertTrue("We expect a non-empty JSON blob", "{}" != data)

        // Parse the data so that we can easily check the other fields
        val jsonData = JSONObject(data)
        val clientInfo = jsonData["client_info"] as JSONObject

        assertEquals(BuildConfig.LIBRARY_VERSION, clientInfo.getString("telemetry_sdk_build"))
    }

    @Test
    fun `collect() must report a valid ping with the data from the engines`() {
        val engine1Data = JSONArray(listOf("1", "2", "3"))
        val engine2Data = JSONArray(listOf("a", "b", "c"))
        val maker = PingMaker(
            StorageEngineManager(
                storageEngines = mapOf(
                    "engine1" to MockStorageEngine(engine1Data),
                    "engine2" to MockStorageEngine(engine2Data),
                    "wontCollect" to MockStorageEngine(JSONObject(), "notThisPing")
                ),
                applicationContext = mockApplicationContext
            ),
            mockApplicationContext
        )

        // Gather the data, this should have everything in the 'test' ping which is the default
        // storex
        val data = maker.collect(customPing).orEmpty()
        assertNotNull("We expect a non-null JSON blob", data)

        // Parse the data so that we can easily check the other fields
        val jsonData = JSONObject(data)
        val metricsData = jsonData.getJSONObject("metrics")
        assertEquals(engine1Data, metricsData.getJSONArray("engine1"))
        assertEquals(engine2Data, metricsData.getJSONArray("engine2"))
        assertFalse(metricsData.has("wontCollect"))
    }

    @Test
    fun `collect() must report an empty string when no data is stored`() {
        val engine1Data = JSONArray(listOf("1", "2", "3"))
        val engine2Data = JSONArray(listOf("a", "b", "c"))
        val maker = PingMaker(
            StorageEngineManager(
                storageEngines = mapOf(
                    "engine1" to MockStorageEngine(engine1Data),
                    "engine2" to MockStorageEngine(engine2Data)
                ),
                applicationContext = mockApplicationContext
            ),
            mockApplicationContext
        )

        val noSuchPing = PingType(
            name = "noSuchPing",
            includeClientId = true
        )

        // Gather the data. We expect an empty string
        val data = maker.collect(noSuchPing)
        assertNull("We expect an empty string", data)
    }

    @Test
    fun `seq number must be sequential`() {
        // NOTE: Using a "real" ApplicationContext here so that it will have
        // a working SharedPreferences implementation
        val applicationContext = ApplicationProvider.getApplicationContext<Context>()
        val maker = PingMaker(
            StorageEngineManager(
                storageEngines = mapOf(
                    "engine1" to MockStorageEngine(JSONObject(mapOf("a.b" to "foo")), "test1"),
                    "engine2" to MockStorageEngine(JSONObject(mapOf("c.d" to "foo")), "test2")
                ),
                applicationContext = applicationContext
            ),
            applicationContext
        )

        // Clear the sharedPreferences on the PingMaker so we can test that the
        // numbers start at zero.
        maker.sharedPreferences?.let {
            val editor = it.edit()
            editor.clear()
            editor.apply()
        }
        // Collect pings, and make sure the seq numbers within each ping type
        // are sequential
        val results = mutableListOf<Int>()
        for (i in 1..2) {
            for (pingName in arrayOf("test1", "test2")) {
                val ping = PingType(
                    name = pingName,
                    includeClientId = true
                )
                val data = maker.collect(ping).orEmpty()
                val jsonData = JSONObject(data)
                val pingInfo = jsonData["ping_info"] as JSONObject
                val seqNum = pingInfo.getInt("seq")
                results.add(seqNum)
            }
        }

        assertEquals(results[0], 0)
        assertEquals(results[1], 0)
        assertEquals(results[2], 1)
        assertEquals(results[3], 1)
    }
}

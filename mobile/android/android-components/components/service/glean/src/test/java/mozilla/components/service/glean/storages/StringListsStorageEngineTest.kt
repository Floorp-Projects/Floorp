/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.error.ErrorRecording.testGetNumRecordedErrors
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.StringListMetricType
import mozilla.components.service.glean.testing.GleanTestRule
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class StringListsStorageEngineTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `set() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_list_metric",
            sendInPings = storeNames
        )

        val list = listOf("First", "Second")

        StringListsStorageEngine.set(
            metricData = metric,
            value = list
        )

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            val snapshot = StringListsStorageEngine.getSnapshot(
                storeName = storeName,
                clearStore = false
            )
            assertEquals(1, snapshot!!.size)
            assertEquals("First", snapshot["telemetry.string_list_metric"]?.get(0))
            assertEquals("Second", snapshot["telemetry.string_list_metric"]?.get(1))
        }
    }

    @Test
    fun `add() properly adds the value in all stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_list_metric",
            sendInPings = storeNames
        )

        StringListsStorageEngine.add(
            metricData = metric,
            value = "First"
        )

        // Check that the data was correctly added in each store.
        for (storeName in storeNames) {
            val snapshot = StringListsStorageEngine.getSnapshot(
                storeName = storeName,
                clearStore = false)
            assertEquals("First", snapshot!!["telemetry.string_list_metric"]?.get(0))
        }
    }

    @Test
    fun `add() won't allow adding beyond the max list length in a single accumulation`() {
        val metric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )

        for (i in 1..StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE + 1) {
            StringListsStorageEngine.add(
                metricData = metric,
                value = "value$i"
            )
        }

        // Check that list was truncated.
        val snapshot = StringListsStorageEngine.getSnapshot(
            storeName = "store1",
            clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_list_metric"))
        assertEquals(
            StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE,
            snapshot["telemetry.string_list_metric"]?.count()
        )

        assertEquals(1, testGetNumRecordedErrors(metric, ErrorType.InvalidValue))
    }

    @Test
    fun `add() won't allow adding beyond the max list length over multiple accumulations`() {
        val metric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )

        // Add values up to half capacity
        for (i in 1..StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE / 2) {
            StringListsStorageEngine.add(
                metricData = metric,
                value = "value$i"
            )
        }

        // Check that list was added
        val snapshot = StringListsStorageEngine.getSnapshot(
            storeName = "store1",
            clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_list_metric"))
        assertEquals(
            StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE / 2,
            snapshot["telemetry.string_list_metric"]?.count()
        )

        // Add values that would exceed capacity
        for (i in 1..StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE) {
            StringListsStorageEngine.add(
                metricData = metric,
                value = "otherValue$i"
            )
        }

        // Check that the list was truncated to the list capacity
        val snapshot2 = StringListsStorageEngine.getSnapshot(
            storeName = "store1",
            clearStore = false)
        assertEquals(1, snapshot2!!.size)
        assertEquals(true, snapshot2.containsKey("telemetry.string_list_metric"))
        assertEquals(
            StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE,
            snapshot2["telemetry.string_list_metric"]?.count()
        )

        assertEquals(StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE / 2,
            testGetNumRecordedErrors(metric, ErrorType.InvalidValue))
    }

    @Test
    fun `set() won't allow adding a list longer than the max list length`() {
        val metric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )

        val stringList: MutableList<String> = mutableListOf()
        for (i in 1..StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE + 1) {
            stringList.add("value$i")
        }

        StringListsStorageEngine.set(metricData = metric, value = stringList)

        // Check that list was truncated.
        val snapshot = StringListsStorageEngine.getSnapshot(
            storeName = "store1",
            clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_list_metric"))
        assertEquals(
            StringListsStorageEngineImplementation.MAX_LIST_LENGTH_VALUE,
            snapshot["telemetry.string_list_metric"]?.count()
        )

        assertEquals(1, testGetNumRecordedErrors(metric, ErrorType.InvalidValue))
    }

    @Test
    fun `set() records an error and returns when passed an empty list`() {
        val metric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )

        StringListsStorageEngine.set(metricData = metric, value = listOf())

        // If nothing was stored, then the snapshot should be null
        val snapshot = StringListsStorageEngine.getSnapshot(
            storeName = "store1",
            clearStore = false)
        assertNull("Empty list must not be stored", snapshot)

        // Verify the error was recorded
        assertEquals(1, testGetNumRecordedErrors(metric, ErrorType.InvalidValue))
    }

    @Test
    fun `string list deserializer should correctly parse string lists`() {
        val persistedSample = mapOf(
            "store1#telemetry.invalid_string" to "invalid_string",
            "store1#telemetry.invalid_bool" to false,
            "store1#telemetry.null" to null,
            "store1#telemetry.invalid_int" to -1,
            "store1#telemetry.invalid_list" to listOf("1", "2", "3"),
            "store1#telemetry.invalid_int_list" to "[1,2,3]",
            "store1#telemetry.valid" to "[\"a\",\"b\",\"c\"]"
        )

        val storageEngine = StringListsStorageEngineImplementation()

        // Create a fake application context that will be used to load our data.
        val context = Mockito.mock(Context::class.java)
        val sharedPreferences = Mockito.mock(SharedPreferences::class.java)
        Mockito.`when`(sharedPreferences.all).thenAnswer { persistedSample }
        Mockito.`when`(context.getSharedPreferences(
            ArgumentMatchers.eq(storageEngine::class.java.canonicalName),
            ArgumentMatchers.eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)
        Mockito.`when`(context.getSharedPreferences(
            ArgumentMatchers.eq("${storageEngine::class.java.canonicalName}.PingLifetime"),
            ArgumentMatchers.eq(Context.MODE_PRIVATE)
        )).thenReturn(ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences("${storageEngine::class.java.canonicalName}.PingLifetime",
                Context.MODE_PRIVATE))

        storageEngine.applicationContext = context
        val snapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = true)
        // Because JSONArray constructor will deserialize with or without the escaped quotes, it
        // treat the invalid_int_list above the same as the valid list, so we assertEquals 2
        assertEquals(2, snapshot!!.size)
        assertEquals(listOf("a", "b", "c"), snapshot["telemetry.valid"])
    }

    @Test
    fun `string list serializer should correctly serialize lists`() {
        run {
            val storageEngine = StringListsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val storeNames = listOf("store1", "store2")

            val metric = StringListMetricType(
                    disabled = false,
                    category = "telemetry",
                    lifetime = Lifetime.User,
                    name = "string_list_metric",
                    sendInPings = storeNames
            )

            val list = listOf("First", "Second")

            storageEngine.set(
                    metricData = metric,
                    value = list
            )

            // Get snapshot from store1
            val json = storageEngine.getSnapshotAsJSON("store1", true)
            // Check for correct JSON serialization
            assertEquals(
                    "{\"telemetry.string_list_metric\":[\"First\",\"Second\"]}",
                    json.toString()
            )
        }

        // Create a new instance of storage engine to verify serialization to storage rather than
        // to the cache
        run {
            val storageEngine = StringListsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            // Get snapshot from store1
            val json = storageEngine.getSnapshotAsJSON("store1", true)
            // Check for correct JSON serialization
            assertEquals(
                    "{\"telemetry.string_list_metric\":[\"First\",\"Second\"]}",
                    json.toString()
            )
        }
    }

    @Test
    fun `test JSON output`() {
        val storeNames = listOf("store1", "store2")

        val metric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_list_metric",
            sendInPings = storeNames
        )

        val list = listOf("First", "Second")

        StringListsStorageEngine.set(
            metricData = metric,
            value = list
        )

        // Get snapshot from store1 and clear it
        val json = StringListsStorageEngine.getSnapshotAsJSON("store1", true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        Assert.assertNull("The engine must report 'null' on empty stores",
                StringListsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = false))
        // Check for correct JSON serialization
        assertEquals(
            "{\"telemetry.string_list_metric\":[\"First\",\"Second\"]}",
            json.toString()
        )
    }

    @Test
    fun `The API truncates long string values`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1"
        val stringListMetric = StringListMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_list_metric",
            sendInPings = listOf("store1")
        )

        val longString = "a".repeat(StringListsStorageEngineImplementation.MAX_STRING_LENGTH + 10)

        // Check that data was truncated via add() method.
        StringListsStorageEngine.add(stringListMetric, longString)
        var snapshot = StringListsStorageEngine.getSnapshotAsJSON("store1", true) as JSONObject
        var stringList = snapshot["telemetry.string_list_metric"] as JSONArray
        assertEquals(longString.take(StringListsStorageEngineImplementation.MAX_STRING_LENGTH),
            stringList[0])

        // Check that data was truncated via set() method.
        StringListsStorageEngine.set(stringListMetric, listOf(longString))
        snapshot = StringListsStorageEngine.getSnapshotAsJSON("store1", true) as JSONObject
        stringList = snapshot["telemetry.string_list_metric"] as JSONArray
        assertEquals(1, stringList.length())
        assertTrue(stringListMetric.testHasValue())
        assertEquals(longString.take(StringListsStorageEngineImplementation.MAX_STRING_LENGTH),
            stringList[0])

        assertEquals(2, testGetNumRecordedErrors(stringListMetric, ErrorType.InvalidValue))
    }
}

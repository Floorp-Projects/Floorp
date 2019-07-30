/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.error.ErrorRecording.testGetNumRecordedErrors
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.StringMetricType
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class StringsStorageEngineTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `string deserializer should correctly parse strings`() {
        val persistedSample = mapOf(
            "store1#telemetry.invalid_number" to 1,
            "store1#telemetry.invalid_bool" to false,
            "store1#telemetry.null" to null,
            "store1#telemetry.valid" to "test"
        )

        val storageEngine = StringsStorageEngineImplementation()

        // Create a fake application context that will be used to load our data.
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.all).thenAnswer { persistedSample }
        `when`(context.getSharedPreferences(
            eq(storageEngine::class.java.canonicalName),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)
        `when`(context.getSharedPreferences(
            eq("${storageEngine::class.java.canonicalName}.PingLifetime"),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences("${storageEngine::class.java.canonicalName}.PingLifetime",
                Context.MODE_PRIVATE))

        storageEngine.applicationContext = context
        val snapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = true)
        assertEquals(1, snapshot!!.size)
        assertEquals("test", snapshot["telemetry.valid"])
    }

    @Test
    fun `string serializer should correctly serialize strings`() {
        run {
            val storageEngine = StringsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val metric = StringMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "string_metric",
                sendInPings = listOf("store1")
            )

            // Record the string in the store, without providing optional arguments.
            storageEngine.record(
                metric,
                value = "test_string_value"
            )

            // Get the snapshot from "store1"
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1",
                clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.string_metric\":\"test_string_value\"}",
                snapshot.toString())
        }

        // Create a new instance of storage engine to verify serialization to storage rather than
        // to the cache
        run {
            val storageEngine = StringsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            // Get the snapshot from "store1"
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1",
                clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.string_metric\":\"test_string_value\"}",
                snapshot.toString())
        }
    }

    @Test
    fun `setValue() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_metric",
            sendInPings = storeNames
        )

        // Record the string in the stores, without providing optional arguments.
        StringsStorageEngine.record(
            metric,
            value = "test_string_object"
        )

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            val snapshot = StringsStorageEngine.getSnapshot(storeName = storeName, clearStore = false)
            assertEquals(1, snapshot!!.size)
            assertEquals("test_string_object", snapshot.get("telemetry.string_metric"))
        }
    }

    @Test
    fun `getSnapshot() returns null if nothing is recorded in the store`() {
        assertNull("The engine must report 'null' on empty or unknown stores",
            StringsStorageEngine.getSnapshot(storeName = "unknownStore", clearStore = false))
    }

    @Test
    fun `getSnapshot() correctly clears the stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_metric",
            sendInPings = storeNames
        )

        // Record the string in the stores, without providing optional arguments.
        StringsStorageEngine.record(
            metric,
            value = "test_string_value"
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false))

        // Check that we get the right data from both the stores. Clearing "store1" must
        // not clear "store2" as well.
        val snapshot2 = StringsStorageEngine.getSnapshot(storeName = "store2", clearStore = false)
        for (s in listOf(snapshot, snapshot2)) {
            assertEquals(1, s!!.size)
        }
    }

    @Test
    fun `Strings are serialized in the correct JSON format`() {
        val metric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_metric",
            sendInPings = listOf("store1")
        )

        // Record the string in the store, without providing optional arguments.
        StringsStorageEngine.record(
            metric,
            value = "test_string_value"
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = StringsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            StringsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = false))
        // Check that this serializes to the expected JSON format.
        assertEquals("{\"telemetry.string_metric\":\"test_string_value\"}",
            snapshot.toString())
    }

    @Test
    fun `The API truncates long string values`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1"
        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_metric",
            sendInPings = listOf("store1")
        )

        val testString = "012345678901234567890".repeat(20)
        val expectedString = testString.take(StringsStorageEngineImplementation.MAX_LENGTH_VALUE)

        stringMetric.set(testString)
        // Check that data was truncated.
        assertTrue(stringMetric.testHasValue())
        assertEquals(
            expectedString,
            stringMetric.testGetValue()
        )

        assertEquals(1, testGetNumRecordedErrors(stringMetric, ErrorType.InvalidValue))
    }
}

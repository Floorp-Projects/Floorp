/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.CounterMetricType
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.error.ErrorRecording.testGetNumRecordedErrors
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertFalse
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CountersStorageEngineTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `counter deserializer should correctly parse integers`() {
        val persistedSample = mapOf(
            "store1#telemetry.invalid_string" to "invalid_string",
            "store1#telemetry.invalid_bool" to false,
            "store1#telemetry.null" to null,
            "store1#telemetry.invalid_int" to -1,
            "store1#telemetry.valid" to 1
        )

        val storageEngine = CountersStorageEngineImplementation()

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
        assertEquals(1, snapshot["telemetry.valid"])
    }

    @Test
    fun `counter serializer should correctly serialize counters`() {
        run {
            val storageEngine = CountersStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val metric = CounterMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "counter_metric",
                sendInPings = listOf("store1")
            )

            // Record the counter in the store
            storageEngine.record(
                metric,
                amount = 1
            )

            // Get the snapshot from "store1" and clear it.
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.counter_metric\":1}", snapshot.toString())
        }

        // Re-instantiate storage engine to validate serialization from storage rather than cache
        run {
            val storageEngine = CountersStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            // Get the snapshot from "store1" and clear it.
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.counter_metric\":1}", snapshot.toString())
        }
    }

    @Test
    fun `setValue() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = CounterMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "counter_metric",
            sendInPings = storeNames
        )

        // Record the counter in the stores, without providing optional arguments.
        CountersStorageEngine.record(
            metric,
            amount = 1
        )

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            val snapshot = CountersStorageEngine.getSnapshot(storeName = storeName, clearStore = false)
            assertEquals(1, snapshot!!.size)
            assertEquals(1, snapshot.get("telemetry.counter_metric"))
        }
    }

    @Test
    fun `getSnapshot() returns null if nothing is recorded in the store`() {
        assertNull("The engine must report 'null' on empty or unknown stores",
                CountersStorageEngine.getSnapshot(storeName = "unknownStore", clearStore = false))
    }

    @Test
    fun `getSnapshot() correctly clears the stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = CounterMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "counter_metric",
            sendInPings = storeNames
        )

        // Record the counter in the stores, without providing optional arguments.
        CountersStorageEngine.record(
            metric,
            amount = 1
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = CountersStorageEngine.getSnapshot(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
                CountersStorageEngine.getSnapshot(storeName = "store1", clearStore = false))

        // Check that we get the right data from both the stores. Clearing "store1" must
        // not clear "store2" as well.
        val snapshot2 = CountersStorageEngine.getSnapshot(storeName = "store2", clearStore = false)
        for (s in listOf(snapshot, snapshot2)) {
            assertEquals(1, s!!.size)
        }
    }

    @Test
    fun `counters are serialized in the correct JSON format`() {
        val metric = CounterMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "counter_metric",
            sendInPings = listOf("store1")
        )

        // Record the counter in the store
        CountersStorageEngine.record(
            metric,
            amount = 1
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = CountersStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            CountersStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = false))
        // Check that this serializes to the expected JSON format.
        assertEquals("{\"telemetry.counter_metric\":1}",
            snapshot.toString())
    }

    @Test
    fun `counters must not increment when passed zero or negative`() {
        // Define a 'counterMetric' counter metric, which will be stored in "store1".
        val counterMetric = CounterMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "counter_metric",
            sendInPings = listOf("store1")
        )

        // Attempt to increment the counter with zero
        counterMetric.add(0)
        // Check that nothing was recorded.
        assertFalse("Counters must not be recorded if incremented with zero",
            counterMetric.testHasValue())

        // Attempt to increment the counter with negative
        counterMetric.add(-1)
        // Check that nothing was recorded.
        assertFalse("Counters must not be recorded if incremented with negative",
            counterMetric.testHasValue())

        // Make sure that the errors have been recorded
        assertEquals(2, testGetNumRecordedErrors(counterMetric, ErrorType.InvalidValue))
    }
}

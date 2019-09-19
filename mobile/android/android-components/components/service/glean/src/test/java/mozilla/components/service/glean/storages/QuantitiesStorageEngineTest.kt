/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.collectAndCheckPingSchema
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.QuantityMetricType
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.error.ErrorRecording.testGetNumRecordedErrors
import mozilla.components.service.glean.private.PingType
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
class QuantitiesStorageEngineTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `quantity deserializer should correctly parse integers`() {
        val persistedSample = mapOf(
            "store1#telemetry.invalid_string" to "invalid_string",
            "store1#telemetry.invalid_bool" to false,
            "store1#telemetry.null" to null,
            "store1#telemetry.invalid_int" to -1,
            "store1#telemetry.valid" to 1L
        )

        val storageEngine = QuantitiesStorageEngineImplementation()

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
        assertEquals(1L, snapshot["telemetry.valid"])
    }

    @Test
    fun `quantity serializer should correctly serialize quantities`() {
        run {
            val storageEngine = QuantitiesStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val metric = QuantityMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "quantity_metric",
                sendInPings = listOf("store1")
            )

            // Record the quantity in the store
            storageEngine.record(
                metric,
                value = 1
            )

            // Get the snapshot from "store1" and clear it.
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.quantity_metric\":1}", snapshot.toString())
        }

        // Re-instantiate storage engine to validate serialization from storage rather than cache
        run {
            val storageEngine = QuantitiesStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            // Get the snapshot from "store1" and clear it.
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.quantity_metric\":1}", snapshot.toString())
        }
    }

    @Test
    fun `setValue() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = QuantityMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "quantity_metric",
            sendInPings = storeNames
        )

        // Record the quantity in the stores, without providing optional arguments.
        QuantitiesStorageEngine.record(
            metric,
            value = 1
        )

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            val snapshot = QuantitiesStorageEngine.getSnapshot(storeName = storeName, clearStore = false)
            assertEquals(1, snapshot!!.size)
            assertEquals(1L, snapshot.get("telemetry.quantity_metric"))
        }
    }

    @Test
    fun `getSnapshot() returns null if nothing is recorded in the store`() {
        assertNull("The engine must report 'null' on empty or unknown stores",
                QuantitiesStorageEngine.getSnapshot(storeName = "unknownStore", clearStore = false))
    }

    @Test
    fun `getSnapshot() correctly clears the stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = QuantityMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "quantity_metric",
            sendInPings = storeNames
        )

        // Record the quantity in the stores, without providing optional arguments.
        QuantitiesStorageEngine.record(
            metric,
            value = 1
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = QuantitiesStorageEngine.getSnapshot(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
                QuantitiesStorageEngine.getSnapshot(storeName = "store1", clearStore = false))

        // Check that we get the right data from both the stores. Clearing "store1" must
        // not clear "store2" as well.
        val snapshot2 = QuantitiesStorageEngine.getSnapshot(storeName = "store2", clearStore = false)
        for (s in listOf(snapshot, snapshot2)) {
            assertEquals(1, s!!.size)
        }
    }

    @Test
    fun `quantities are serialized in the correct JSON format`() {
        val metric = QuantityMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "quantity_metric",
            sendInPings = listOf("store1")
        )

        // Record the quantity in the store
        QuantitiesStorageEngine.record(
            metric,
            value = 1
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = QuantitiesStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            QuantitiesStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = false))
        // Check that this serializes to the expected JSON format.
        assertEquals("{\"telemetry.quantity_metric\":1}",
            snapshot.toString())
    }

    @Test
    fun `quantities are serialized in a form that validates against the schema`() {
        val pingType = PingType("store1", true)

        val metric = QuantityMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "quantity_metric",
            sendInPings = listOf("store1")
        )

        // Record the quantity in the store
        QuantitiesStorageEngine.record(
            metric,
            value = 1
        )

        collectAndCheckPingSchema(pingType)
    }

    @Test
    fun `quantities must not set when passed negative`() {
        // Define a 'quantityMetric' quantity metric, which will be stored in "store1".
        val quantityMetric = QuantityMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "quantity_metric",
            sendInPings = listOf("store1")
        )

        // Attempt to set the quantity to negative
        quantityMetric.set(-1)
        // Check that nothing was recorded.
        assertFalse("Quantities must not be recorded if set with negative",
            quantityMetric.testHasValue())

        // Make sure that the errors have been recorded
        assertEquals(1, testGetNumRecordedErrors(quantityMetric, ErrorType.InvalidValue))
    }
}

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.BooleanMetricType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BooleansStorageEngineTest {

    @Before
    fun setUp() {
        BooleansStorageEngine.applicationContext = ApplicationProvider.getApplicationContext()
        BooleansStorageEngine.clearAllStores()
    }

    @Test
    fun `boolean deserializer should correctly parse booleans`() {
        val persistedSample = mapOf(
            "store1#telemetry.invalid_number" to 1,
            "store1#telemetry.bool" to true,
            "store1#telemetry.null" to null,
            "store1#telemetry.invalid_string" to "test"
        )

        val storageEngine = BooleansStorageEngineImplementation()

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
        assertEquals(true, snapshot["telemetry.bool"])
    }

    @Test
    fun `boolean serializer should correctly serialize booleans`() {
        run {
            val storageEngine = BooleansStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val metric = BooleanMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "boolean_metric",
                sendInPings = listOf("store1")
            )

            // Record the boolean in the store, without providing optional arguments.
            storageEngine.record(
                metric,
                value = true
            )

            // Get the snapshot from "store1"
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1",
                clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.boolean_metric\":true}",
                snapshot.toString())
        }

        // Create a new instance of storage engine to verify serialization to storage rather than
        // to the cache
        run {
            val storageEngine = BooleansStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            // Get the snapshot from "store1"
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1",
                clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.boolean_metric\":true}",
                snapshot.toString())
        }
    }

    @Test
    fun `setValue() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "boolean_metric",
            sendInPings = storeNames
        )

        // Record the boolean in the stores, without providing optional arguments.
        BooleansStorageEngine.record(
            metric,
            value = true
        )

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            val snapshot = BooleansStorageEngine.getSnapshot(storeName = storeName, clearStore = false)
            assertEquals(1, snapshot!!.size)
            assertEquals(true, snapshot.get("telemetry.boolean_metric"))
        }
    }

    @Test
    fun `getSnapshot() returns null if nothing is recorded in the store`() {
        assertNull("The engine must report 'null' on empty or unknown stores",
            BooleansStorageEngine.getSnapshot(storeName = "unknownStore", clearStore = false))
    }

    @Test
    fun `getSnapshot() correctly clears the stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "boolean_metric",
            sendInPings = storeNames
        )

        // Record the boolean in the stores, without providing optional arguments.
        BooleansStorageEngine.record(
            metric,
            value = true
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = BooleansStorageEngine.getSnapshot(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            BooleansStorageEngine.getSnapshot(storeName = "store1", clearStore = false))

        // Check that we get the right data from both the stores. Clearing "store1" must
        // not clear "store2" as well.
        val snapshot2 = BooleansStorageEngine.getSnapshot(storeName = "store2", clearStore = false)
        for (s in listOf(snapshot, snapshot2)) {
            assertEquals(1, s!!.size)
        }
    }

    @Test
    fun `Booleans are serialized in the correct JSON format`() {
        val metric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "boolean_metric",
            sendInPings = listOf("store1")
        )

        // Record the boolean in the store, without providing optional arguments.
        BooleansStorageEngine.record(
            metric,
            value = true
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = BooleansStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            BooleansStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = false))
        // Check that this serializes to the expected JSON format.
        assertEquals("{\"telemetry.boolean_metric\":true}",
            snapshot.toString())
    }
}

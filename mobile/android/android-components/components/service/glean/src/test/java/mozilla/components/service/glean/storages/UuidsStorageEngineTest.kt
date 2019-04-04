/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.UuidMetricType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
class UuidsStorageEngineTest {

    @Before
    fun setUp() {
        UuidsStorageEngine.applicationContext = ApplicationProvider.getApplicationContext()
        UuidsStorageEngine.clearAllStores()
    }

    @Test
    fun `uuid deserializer should correctly parse UUIDs`() {
        val persistedSample = mapOf(
            "store1#telemetry.invalid_number" to 1,
            "store1#telemetry.invalid_bool" to false,
            "store1#telemetry.invalid_string" to "c4ff33",
            "store1#telemetry.valid" to "ce2adeb8-843a-4232-87a5-a099ed1e7bb3"
        )

        val storageEngine = UuidsStorageEngineImplementation()

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
        assertEquals("ce2adeb8-843a-4232-87a5-a099ed1e7bb3", snapshot["telemetry.valid"].toString())
    }

    @Test
    fun `UUID serializer correctly serializes UUID's`() {
        run {
            val storageEngine = UuidsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val testUUID = "ce2adeb8-843a-4232-87a5-a099ed1e7bb3"

            val metric = UuidMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "uuid_metric",
                sendInPings = listOf("store1")
            )

            // Record the string in the store, without providing optional arguments.
            storageEngine.record(
                metric,
                value = UUID.fromString(testUUID)
            )

            // Get the snapshot from "store1"
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
            assertEquals("{\"telemetry.uuid_metric\":\"$testUUID\"}",
                snapshot.toString())
        }

        // Create a new instance of storage engine to verify serialization to storage rather than
        // to the cache
        run {
            val storageEngine = UuidsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val testUUID = "ce2adeb8-843a-4232-87a5-a099ed1e7bb3"

            // Get the snapshot from "store1"
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
            assertEquals("{\"telemetry.uuid_metric\":\"$testUUID\"}",
                snapshot.toString())
        }
    }

    @Test
    fun `setValue() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")
        val uuid = UUID.fromString("ce2adeb8-843a-4232-87a5-a099ed1e7bb3")

        val metric = UuidMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "uuid_metric",
            sendInPings = storeNames
        )

        // Record the uuid in the stores, without providing optional arguments.
        UuidsStorageEngine.record(
            metric,
            value = uuid
        )

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            val snapshot = UuidsStorageEngine.getSnapshot(storeName = storeName, clearStore = false)
            assertEquals(1, snapshot!!.size)
            assertEquals(uuid, snapshot.get("telemetry.uuid_metric"))
        }
    }

    @Test
    fun `getSnapshot() returns null if nothing is recorded in the store`() {
        assertNull("The engine must report 'null' on empty or unknown stores",
            UuidsStorageEngine.getSnapshot(storeName = "unknownStore", clearStore = false))
    }

    @Test
    fun `getSnapshot() correctly clears the stores`() {
        val storeNames = listOf("store1", "store2")

        val uuid = UUID.fromString("ce2adeb8-843a-4232-87a5-a099ed1e7bb3")

        val metric = UuidMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "uuid_metric",
            sendInPings = storeNames
        )

        // Record the uuid in the stores, without providing optional arguments.
        UuidsStorageEngine.record(
            metric,
            value = uuid
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = UuidsStorageEngine.getSnapshot(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            UuidsStorageEngine.getSnapshot(storeName = "store1", clearStore = false))

        // Check that we get the right data from both the stores. Clearing "store1" must
        // not clear "store2" as well.
        val snapshot2 = UuidsStorageEngine.getSnapshot(storeName = "store2", clearStore = false)
        for (s in listOf(snapshot, snapshot2)) {
            assertEquals(1, s!!.size)
        }
    }

    @Test
    fun `Uuids are serialized in the correct JSON format`() {
        val testUUID = "ce2adeb8-843a-4232-87a5-a099ed1e7bb3"

        val metric = UuidMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "uuid_metric",
            sendInPings = listOf("store1")
        )

        // Record the string in the store, without providing optional arguments.
        UuidsStorageEngine.record(
            metric,
            value = UUID.fromString(testUUID)
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = UuidsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            UuidsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = false))
        // Check that this serializes to the expected JSON format.
        assertEquals("{\"telemetry.uuid_metric\":\"$testUUID\"}",
            snapshot.toString())
    }

    @Test
    fun `uuids with 'user' lifetime must be correctly persisted`() {
        val sampleUUID = "decaffde-caff-d3ca-ffd3-caffd3caffd3"

        val storageEngine = UuidsStorageEngineImplementation()
        storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

        val metric = UuidMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "uuidMetric",
            sendInPings = listOf("some_store", "other_store")
        )

        storageEngine.record(
            metric,
            value = UUID.fromString(sampleUUID)
        )

        // Check that the persisted shared prefs contains the expected data.
        val storedData = ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences(storageEngine.javaClass.canonicalName, Context.MODE_PRIVATE)
            .all

        assertEquals(2, storedData.size)
        assertTrue(storedData.containsKey("some_store#telemetry.uuidMetric"))
        assertTrue(storedData.containsKey("other_store#telemetry.uuidMetric"))
        assertEquals(sampleUUID, storedData.getValue("some_store#telemetry.uuidMetric"))
        assertEquals(sampleUUID, storedData.getValue("other_store#telemetry.uuidMetric"))
    }
}

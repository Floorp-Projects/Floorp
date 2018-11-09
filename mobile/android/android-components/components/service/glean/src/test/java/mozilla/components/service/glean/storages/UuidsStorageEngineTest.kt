/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.content.Context
import mozilla.components.service.glean.Lifetime
import java.util.UUID

import org.junit.Before
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class UuidsStorageEngineTest {

    @Before
    fun setUp() {
        UuidsStorageEngine.applicationContext = RuntimeEnvironment.application
        // Clear the stored "user" preferences between tests.
        RuntimeEnvironment.application
            .getSharedPreferences(UuidsStorageEngine.javaClass.simpleName, Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
        UuidsStorageEngine.clearAllStores()
    }

    @Test
    fun `setValue() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")
        val uuid = UUID.fromString("ce2adeb8-843a-4232-87a5-a099ed1e7bb3")

        // Record the uuid in the stores, without providing optional arguments.
        UuidsStorageEngine.record(
                stores = storeNames,
                category = "telemetry",
                name = "uuid_metric",
                lifetime = Lifetime.Ping,
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

        // Record the uuid in the stores, without providing optional arguments.
        UuidsStorageEngine.record(
                stores = storeNames,
                category = "telemetry",
                name = "uuid_metric",
                lifetime = Lifetime.Ping,
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

        // Record the string in the store, without providing optional arguments.
        UuidsStorageEngine.record(
            stores = listOf("store1"),
            category = "telemetry",
            name = "uuid_metric",
            lifetime = Lifetime.Ping,
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
        storageEngine.applicationContext = RuntimeEnvironment.application

        storageEngine.record(
            stores = listOf("some_store", "other_store"),
            category = "telemetry",
            name = "uuidMetric",
            lifetime = Lifetime.User,
            value = UUID.fromString(sampleUUID)
        )

        // Check that the persisted shared prefs contains the expected data.
        val storedData = RuntimeEnvironment.application
            .getSharedPreferences(storageEngine.javaClass.simpleName, Context.MODE_PRIVATE)
            .all

        assertEquals(2, storedData.size)
        assertTrue(storedData.containsKey("some_store#telemetry.uuidMetric"))
        assertTrue(storedData.containsKey("other_store#telemetry.uuidMetric"))
        assertEquals(sampleUUID, storedData.getValue("some_store#telemetry.uuidMetric"))
        assertEquals(sampleUUID, storedData.getValue("other_store#telemetry.uuidMetric"))
    }
}

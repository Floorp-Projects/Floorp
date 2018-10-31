/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import java.util.UUID

import org.junit.Before
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class UuidsStorageEngineTest {

    @Before
    fun setUp() {
        UuidsStorageEngine.clearAllStores()
    }

    @Test
    fun `setValue() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")
        val uuid = UUID.randomUUID()

        // Record the uuid in the stores, without providing optional arguments.
        UuidsStorageEngine.record(
                stores = storeNames,
                category = "telemetry",
                name = "uuid_metric",
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

        val uuid = UUID.randomUUID()

        // Record the uuid in the stores, without providing optional arguments.
        UuidsStorageEngine.record(
                stores = storeNames,
                category = "telemetry",
                name = "uuid_metric",
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
}

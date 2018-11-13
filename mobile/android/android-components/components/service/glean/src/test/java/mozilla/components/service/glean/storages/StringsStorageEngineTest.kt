/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.content.Context
import mozilla.components.service.glean.Lifetime
import org.junit.Before
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class StringsStorageEngineTest {

    @Before
    fun setUp() {
        StringsStorageEngine.applicationContext = RuntimeEnvironment.application
        // Clear the stored "user" preferences between tests.
        RuntimeEnvironment.application
            .getSharedPreferences(StringsStorageEngine.javaClass.simpleName, Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
        StringsStorageEngine.clearAllStores()
    }

    @Test
    fun `setValue() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")

        // Record the string in the stores, without providing optional arguments.
        StringsStorageEngine.record(
                stores = storeNames,
                category = "telemetry",
                name = "string_metric",
                lifetime = Lifetime.Ping,
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

        // Record the string in the stores, without providing optional arguments.
        StringsStorageEngine.record(
            stores = storeNames,
            category = "telemetry",
            name = "string_metric",
            lifetime = Lifetime.Ping,
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
        // Record the string in the store, without providing optional arguments.
        StringsStorageEngine.record(
            stores = listOf("store1"),
            category = "telemetry",
            name = "string_metric",
            lifetime = Lifetime.Ping,
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
}

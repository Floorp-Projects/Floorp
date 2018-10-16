/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import org.junit.Before
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.shadows.ShadowSystemClock

@RunWith(RobolectricTestRunner::class)
class EventsStorageEngineTest {

    @Before
    fun setUp() {
        EventsStorageEngine.clearAllStores()
    }

    @Test
    fun `record() properly records without optional arguments`() {
        val storeNames = listOf("store1", "store2")

        // Record the event in the stores, without providing optional arguments.
        EventsStorageEngine.record(
                stores = storeNames,
                category = "telemetry",
                name = "test_event_no_optional",
                objectId = "test_event_object"
        )

        // Check that the data was correctly recorded in each store.
        for (storeName in storeNames) {
            val snapshot = EventsStorageEngine.getSnapshot(storeName = storeName, clearStore = false)
            assertEquals(1, snapshot!!.size)
            assertEquals("telemetry", snapshot!!.first().category)
            assertEquals("test_event_no_optional", snapshot!!.first().name)
            assertEquals("test_event_object", snapshot!!.first().objectId)
            assertNull("The 'value' must be null if not provided",
                    snapshot!!.first().value)
            assertNull("The 'extra' must be null if not provided",
                    snapshot!!.first().extra)
        }
    }

    @Test
    fun `record() properly records with optional arguments`() {
        val storeNames = listOf("store1", "store2")

        // Record the event in the stores, without providing optional arguments.
        EventsStorageEngine.record(
                stores = storeNames,
                category = "telemetry",
                name = "test_event_with_optional",
                objectId = "test_event_object",
                value = "user_value",
                extra = mapOf("key1" to "value1", "key2" to "value2")
        )

        // Check that the data was correctly recorded in each store.
        for (storeName in storeNames) {
            val snapshot = EventsStorageEngine.getSnapshot(storeName = storeName, clearStore = false)
            assertEquals(1, snapshot!!.size)
            assertEquals("telemetry", snapshot!!.first().category)
            assertEquals("test_event_with_optional", snapshot!!.first().name)
            assertEquals("test_event_object", snapshot!!.first().objectId)
            assertEquals("user_value", snapshot!!.first().value)
            assertEquals(mapOf("key1" to "value1", "key2" to "value2"), snapshot!!.first().extra)
        }
    }

    @Test
    fun `record() computes the correct time since process start`() {
        val expectedTimeSinceStart: Long = 37

        // Sleep a bit Record the event in the store.
        ShadowSystemClock.sleep(expectedTimeSinceStart)
        EventsStorageEngine.record(
                stores = listOf("store1"),
                category = "telemetry",
                name = "test_event_time",
                objectId = "test_event_object"
        )

        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals("telemetry", snapshot!!.first().category)
        assertEquals("test_event_time", snapshot!!.first().name)
        assertEquals("test_event_object", snapshot!!.first().objectId)
        assertNull("The 'value' must be null if not provided",
                snapshot!!.first().value)
        assertNull("The 'extra' must be null if not provided",
                snapshot!!.first().extra)
        assertEquals(expectedTimeSinceStart, snapshot!!.first().msSinceStart)
    }

    @Test
    fun `getSnapshot() returns null if nothing is recorded in the store`() {
        assertNull("The engine must report 'null' on empty or unknown stores",
                EventsStorageEngine.getSnapshot(storeName = "unknownStore", clearStore = false))
    }

    @Test
    fun `getSnapshot() correctly clears the stores`() {
        val storeNames = listOf("store1", "store2")

        // Record the event in the stores, without providing optional arguments.
        EventsStorageEngine.record(
                stores = storeNames,
                category = "telemetry",
                name = "test_event_clear",
                objectId = "test_event_object"
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
                EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false))

        // Check that we get the right data from both the stores. Clearing "store1" must
        // not clear "store2" as well.
        val snapshot2 = EventsStorageEngine.getSnapshot(storeName = "store2", clearStore = false)
        for (s in listOf(snapshot, snapshot2)) {
            assertEquals(1, s!!.size)
            assertEquals("telemetry", s!!.first().category)
            assertEquals("test_event_clear", s!!.first().name)
            assertEquals("test_event_object", s!!.first().objectId)
            assertNull("The 'value' must be null if not provided",
                    s!!.first().value)
            assertNull("The 'extra' must be null if not provided",
                    s!!.first().extra)
        }
    }
}

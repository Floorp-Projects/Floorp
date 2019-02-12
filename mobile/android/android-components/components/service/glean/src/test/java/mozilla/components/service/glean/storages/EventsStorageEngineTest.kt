/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.os.SystemClock
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.checkPingSchema
import mozilla.components.service.glean.Lifetime
import mozilla.components.service.glean.EventMetricType
import mozilla.components.service.glean.FakeDispatchersInTest
import mozilla.components.service.glean.getContextWithMockedInfo
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.resetGlean
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Before
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class EventsStorageEngineTest {

    @get:Rule
    val fakeDispatchers = FakeDispatchersInTest()

    @Before
    fun setUp() {
        Glean.initialized = false
        Glean.initialize(ApplicationProvider.getApplicationContext())
        assert(Glean.initialized)
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
            assertEquals("telemetry", snapshot.first().category)
            assertEquals("test_event_no_optional", snapshot.first().name)
            assertEquals("test_event_object", snapshot.first().objectId)
            assertNull("The 'value' must be null if not provided",
                    snapshot.first().value)
            assertNull("The 'extra' must be null if not provided",
                    snapshot.first().extra)
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
            assertEquals("telemetry", snapshot.first().category)
            assertEquals("test_event_with_optional", snapshot.first().name)
            assertEquals("test_event_object", snapshot.first().objectId)
            assertEquals("user_value", snapshot.first().value)
            assertEquals(mapOf("key1" to "value1", "key2" to "value2"), snapshot.first().extra)
        }
    }

    @Test
    fun `record() computes the correct time since process start`() {
        val expectedTimeSinceStart: Long = 37

        // Sleep a bit Record the event in the store.
        SystemClock.sleep(expectedTimeSinceStart)
        EventsStorageEngine.record(
                stores = listOf("store1"),
                category = "telemetry",
                name = "test_event_time",
                objectId = "test_event_object"
        )

        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals("telemetry", snapshot.first().category)
        assertEquals("test_event_time", snapshot.first().name)
        assertEquals("test_event_object", snapshot.first().objectId)
        assertNull("The 'value' must be null if not provided",
                snapshot.first().value)
        assertNull("The 'extra' must be null if not provided",
                snapshot.first().extra)
        assertEquals(expectedTimeSinceStart, snapshot.first().msSinceStart)
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
            assertEquals("telemetry", s.first().category)
            assertEquals("test_event_clear", s.first().name)
            assertEquals("test_event_object", s.first().objectId)
            assertNull("The 'value' must be null if not provided",
                    s.first().value)
            assertNull("The 'extra' must be null if not provided",
                    s.first().extra)
        }
    }

    @Test
    fun `Events are serialized in the correct JSON format`() {
        // Record the event in the store, without providing optional arguments.
        EventsStorageEngine.record(
            stores = listOf("store1"),
            category = "telemetry",
            name = "test_event_clear",
            objectId = "test_event_object"
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = EventsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            EventsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = false))
        // Check that this serializes to the expected JSON format.
        assertEquals("[[0,\"telemetry\",\"test_event_clear\",\"test_event_object\",null,null]]",
            snapshot.toString())
    }

    @Test
    fun `test sending of event ping when it fills up`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

        EventsStorageEngine.clearAllStores()
        val click = EventMetricType(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "click",
            sendInPings = listOf("default"),
            objects = listOf("buttonA")
        )

        resetGlean(getContextWithMockedInfo(), Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        try {
            // We send 510 events.  We expect to get the first 500 in the ping, and 10 remaining afterward
            for (i in 0..509) {
                click.record("buttonA", "$i")
            }

            val request = server.takeRequest()
            val applicationId = "mozilla-components-service-glean"
            assert(request.path.startsWith("/submit/$applicationId/events/${Glean.SCHEMA_VERSION}/"))
            val eventsJsonData = request.body.readUtf8()
            val eventsJson = checkPingSchema(eventsJsonData)
            val eventsArray = eventsJson.getJSONArray("events")!!
            assertEquals(500, eventsArray.length())

            for (i in 0..499) {
                assertEquals("$i", eventsArray.getJSONArray(i).getString(4))
            }
        } finally {
            server.shutdown()
        }

        val remaining = EventsStorageEngine.getSnapshot("events", false)!!
        assertEquals(10, remaining.size)
        for (i in 0..9) {
            assertEquals("${i + 500}", remaining[i].value)
        }
    }
}

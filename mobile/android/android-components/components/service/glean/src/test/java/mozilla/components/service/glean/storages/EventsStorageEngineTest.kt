/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.os.SystemClock
import androidx.test.core.app.ApplicationProvider
import androidx.work.testing.WorkManagerTestInitHelper
import mozilla.components.service.glean.checkPingSchema
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.error.ErrorRecording.testGetNumRecordedErrors
import mozilla.components.service.glean.Lifetime
import mozilla.components.service.glean.EventMetricType
import mozilla.components.service.glean.FakeDispatchersInTest
import mozilla.components.service.glean.getContextWithMockedInfo
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.resetGlean
import mozilla.components.service.glean.triggerWorkManager
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Rule
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class EventsStorageEngineTest {
    @get:Rule
    @Suppress("EXPERIMENTAL_API_USAGE")
    val fakeDispatchers = FakeDispatchersInTest()

    @Before
    fun setUp() {
        resetGlean()
        assert(Glean.initialized)
        EventsStorageEngine.clearAllStores()

        // Initialize WorkManager using the WorkManagerTestInitHelper.
        WorkManagerTestInitHelper.initializeTestWorkManager(ApplicationProvider.getApplicationContext())
    }

    @Test
    fun `record() properly records without optional arguments`() {
        val storeNames = listOf("store1", "store2")

        val event = EventMetricType(
            disabled = false,
            category = "telemetry",
            name = "test_event_no_optional",
            lifetime = Lifetime.Ping,
            sendInPings = storeNames
        )

        // Record the event in the stores, without providing optional arguments.
        EventsStorageEngine.record(
                metricData = event,
                monotonicElapsedMs = SystemClock.elapsedRealtime()
        )

        // Check that the data was correctly recorded in each store.
        for (storeName in storeNames) {
            val snapshot = EventsStorageEngine.getSnapshot(storeName = storeName, clearStore = false)
            assertEquals(1, snapshot!!.size)
            assertEquals("telemetry", snapshot.first().category)
            assertEquals("test_event_no_optional", snapshot.first().name)
            assertNull("The 'extra' must be null if not provided",
                    snapshot.first().extra)
        }
    }

    @Test
    fun `record() properly records with optional arguments`() {
        val storeNames = listOf("store1", "store2")

        val event = EventMetricType(
            disabled = false,
            category = "telemetry",
            name = "test_event_with_optional",
            lifetime = Lifetime.Ping,
            sendInPings = storeNames,
            allowedExtraKeys = listOf("key1", "key2")
        )

        // Record the event in the stores, providing optional arguments.
        EventsStorageEngine.record(
                metricData = event,
                monotonicElapsedMs = SystemClock.elapsedRealtime(),
                extra = mapOf("key1" to "value1", "key2" to "value2")
        )

        // Check that the data was correctly recorded in each store.
        for (storeName in storeNames) {
            val snapshot = EventsStorageEngine.getSnapshot(storeName = storeName, clearStore = false)
            assertEquals(1, snapshot!!.size)
            assertEquals("telemetry", snapshot.first().category)
            assertEquals("test_event_with_optional", snapshot.first().name)
            assertEquals(mapOf("key1" to "value1", "key2" to "value2"), snapshot.first().extra)
        }
    }

    @Test
    fun `record() computes the correct time since process start`() {
        val expectedTimeSinceStart: Long = 37

        val event = EventMetricType(
            disabled = false,
            category = "telemetry",
            name = "test_event_time",
            lifetime = Lifetime.Ping,
            sendInPings = listOf("store1")
        )

        // Sleep a bit Record the event in the store.
        SystemClock.sleep(expectedTimeSinceStart)
        EventsStorageEngine.record(
            metricData = event,
            monotonicElapsedMs = SystemClock.elapsedRealtime()
        )

        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals("telemetry", snapshot.first().category)
        assertEquals("test_event_time", snapshot.first().name)
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

        val event = EventMetricType(
            disabled = false,
            category = "telemetry",
            name = "test_event_clear",
            lifetime = Lifetime.Ping,
            sendInPings = storeNames
        )

        // Record the event in the stores, without providing optional arguments.
        EventsStorageEngine.record(
                metricData = event,
                monotonicElapsedMs = SystemClock.elapsedRealtime()
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
            assertNull("The 'extra' must be null if not provided",
                    s.first().extra)
        }
    }

    @Test
    fun `Events are serialized in the correct JSON format (no extra)`() {
        val event = EventMetricType(
            disabled = false,
            category = "telemetry",
            name = "test_event_clear",
            lifetime = Lifetime.Ping,
            sendInPings = listOf("store1")
        )

        // Record the event in the store, without providing optional arguments.
        EventsStorageEngine.record(
            metricData = event,
            monotonicElapsedMs = SystemClock.elapsedRealtime()
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = EventsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            EventsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = false))
        // Check that this serializes to the expected JSON format.
        assertEquals("[[0,\"telemetry\",\"test_event_clear\"]]",
            snapshot.toString())
    }

    @Test
    fun `Events are serialized in the correct JSON format (with extra)`() {
        val event = EventMetricType(
            disabled = false,
            category = "telemetry",
            name = "test_event_clear",
            lifetime = Lifetime.Ping,
            sendInPings = listOf("store1"),
            allowedExtraKeys = listOf("someExtra")
        )

        // Record the event in the store, without providing optional arguments.
        EventsStorageEngine.record(
            metricData = event,
            monotonicElapsedMs = SystemClock.elapsedRealtime(),
            extra = mapOf("someExtra" to "field")
        )

        // Get the snapshot from "store1" and clear it.
        val snapshot = EventsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            EventsStorageEngine.getSnapshotAsJSON(storeName = "store1", clearStore = false))
        // Check that this serializes to the expected JSON format.
        assertEquals("[[0,\"telemetry\",\"test_event_clear\",{\"someExtra\":\"field\"}]]",
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
            allowedExtraKeys = listOf("test_event_number")
        )

        resetGlean(getContextWithMockedInfo(), Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        try {
            // We send 510 events.  We expect to get the first 500 in the ping, and 10 remaining afterward
            for (i in 0..509) {
                click.record(extra = mapOf("test_event_number" to "$i"))
            }

            Assert.assertTrue(click.testHasValue())

            // Trigger worker task to upload the pings in the background
            triggerWorkManager()

            val request = server.takeRequest(20L, TimeUnit.SECONDS)
            val applicationId = "mozilla-components-service-glean"
            assert(request.path.startsWith("/submit/$applicationId/events/${Glean.SCHEMA_VERSION}/"))
            val eventsJsonData = request.body.readUtf8()
            val eventsJson = checkPingSchema(eventsJsonData)
            val eventsArray = eventsJson.getJSONArray("events")!!
            assertEquals(500, eventsArray.length())

            for (i in 0..499) {
                assertEquals("$i", eventsArray.getJSONArray(i).getJSONObject(3)["test_event_number"])
            }
        } finally {
            server.shutdown()
        }

        val remaining = EventsStorageEngine.getSnapshot("events", false)!!
        assertEquals(10, remaining.size)
        for (i in 0..9) {
            assertEquals("${i + 500}", remaining[i].extra?.get("test_event_number"))
        }
    }

    @Test
    fun `using 'extra' without declaring allowed keys must not be recorded`() {
        val testEvent = EventMetricType(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "testEvent",
            sendInPings = listOf("store1")
        )

        testEvent.record(
            extra = mapOf("unknownExtra" to "someValue", "unknownExtra2" to "test"))

        // Check that nothing was recorded.
        assertFalse("Events must not be recorded if they use unknown extra keys",
            testEvent.testHasValue())
        assertEquals(1, testGetNumRecordedErrors(testEvent, ErrorType.InvalidValue))
    }

    @Test
    fun `unknown 'extra' keys must not be recorded`() {
        val testEvent = EventMetricType(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "testEvent",
            sendInPings = listOf("store1"),
            allowedExtraKeys = listOf("extra1", "extra2")
        )

        testEvent.record(
            extra = mapOf("unknownExtra" to "someValue", "extra1" to "test"))

        // Check that nothing was recorded.
        assertFalse("Events must not be recorded if they use unknown extra keys",
            testEvent.testHasValue())
        assertEquals(1, testGetNumRecordedErrors(testEvent, ErrorType.InvalidValue))
    }

    @Test
    fun `'extra' keys must be recorded and truncated if needed`() {
        val testEvent = EventMetricType(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "testEvent",
            sendInPings = listOf("store1"),
            allowedExtraKeys = listOf("extra1", "truncatedExtra")
        )

        val testValue = "LeanGleanByFrank"
        testEvent.record(
            extra = mapOf("extra1" to testValue, "truncatedExtra" to testValue.repeat(10)))

        // Check that nothing was recorded.
        val snapshot = testEvent.testGetValue()
        assertEquals(1, snapshot.size)
        assertEquals("ui", snapshot.first().category)
        assertEquals("testEvent", snapshot.first().name)

        assertTrue(
            "'extra' keys must be correctly recorded and truncated",
            mapOf(
                "extra1" to testValue,
                "truncatedExtra" to (testValue.repeat(10)).substring(0, EventsStorageEngine.MAX_LENGTH_EXTRA_KEY_VALUE)
            ) == snapshot.first().extra)
        assertEquals(1, testGetNumRecordedErrors(testEvent, ErrorType.InvalidValue))
    }
}

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.os.SystemClock
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.Dispatchers
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.checkPingSchema
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.error.ErrorRecording.testGetNumRecordedErrors
import mozilla.components.service.glean.getContextWithMockedInfo
import mozilla.components.service.glean.getMockWebServer
import mozilla.components.service.glean.private.EventMetricType
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.NoExtraKeys
import mozilla.components.service.glean.resetGlean
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.service.glean.triggerWorkManager
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.util.concurrent.TimeUnit

// Declared here, since Kotlin can not declare nested enum classes
enum class ExtraKeys {
    Key1,
    Key2
}

enum class SomeExtraKeys {
    SomeExtra
}

enum class TestEventNumberKeys {
    TestEventNumber
}

enum class TruncatedKeys {
    Extra1,
    TruncatedExtra
}

@RunWith(RobolectricTestRunner::class)
class EventsStorageEngineTest {
    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Before
    fun setUp() {
        assert(Glean.initialized)
        EventsStorageEngine.clearAllStores()
    }

    @Test
    fun `record() properly records without optional arguments`() {
        val storeNames = listOf("store1", "store2")

        val event = EventMetricType<NoExtraKeys>(
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

        val event = EventMetricType<ExtraKeys>(
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
    fun `record() computes the correct time between events`() {
        val delayTime: Long = 37

        val event = EventMetricType<NoExtraKeys>(
            disabled = false,
            category = "telemetry",
            name = "test_event_time",
            lifetime = Lifetime.Ping,
            sendInPings = listOf("store1")
        )

        // Sleep a bit Record the event in the store.
        EventsStorageEngine.record(
            metricData = event,
            monotonicElapsedMs = SystemClock.elapsedRealtime()
        )
        SystemClock.sleep(delayTime)
        EventsStorageEngine.record(
            metricData = event,
            monotonicElapsedMs = SystemClock.elapsedRealtime()
        )

        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(2, snapshot!!.size)
        assertEquals("telemetry", snapshot.first().category)
        assertEquals("test_event_time", snapshot.first().name)
        assertNull("The 'extra' must be null if not provided",
                snapshot.first().extra)
        assertEquals(0, snapshot.first().timestamp)
        assertEquals(delayTime, snapshot[1].timestamp)
    }

    @Test
    fun `getSnapshot() returns null if nothing is recorded in the store`() {
        assertNull("The engine must report 'null' on empty or unknown stores",
                EventsStorageEngine.getSnapshot(storeName = "unknownStore", clearStore = false))
    }

    @Test
    fun `getSnapshot() correctly clears the stores`() {
        val storeNames = listOf("store1", "store2")

        val event = EventMetricType<NoExtraKeys>(
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
        EventsStorageEngine.testWaitForWrites()

        // Get the snapshot from "store1" and clear it.
        val snapshot = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = true)
        EventsStorageEngine.testWaitForWrites()
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
                EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false))
        val files = EventsStorageEngine.storageDirectory.listFiles()!!
        assertEquals(1, files.size)
        assertEquals(
            "There should be no events on disk for store1, but there are for store2",
            "store2",
            EventsStorageEngine.storageDirectory.listFiles()?.first()?.name
        )

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
        val event = EventMetricType<NoExtraKeys>(
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
        assertEquals("[{\"timestamp\":0,\"category\":\"telemetry\",\"name\":\"test_event_clear\"}]",
            snapshot.toString())
    }

    @Test
    fun `Events are serialized in the correct JSON format (with extra)`() {
        val event = EventMetricType<SomeExtraKeys>(
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
        assertEquals("[{\"timestamp\":0,\"category\":\"telemetry\",\"name\":\"test_event_clear\",\"extra\":{\"someExtra\":\"field\"}}]",
            snapshot.toString())
    }

    @Test
    fun `test sending of event ping when it fills up`() {
        val server = getMockWebServer()

        val click = EventMetricType<TestEventNumberKeys>(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "click",
            sendInPings = listOf("events"),
            allowedExtraKeys = listOf("test_event_number")
        )

        resetGlean(getContextWithMockedInfo(), Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        try {
            // We send 510 events.  We expect to get the first 500 in the ping, and 10 remaining afterward
            for (i in 0..509) {
                click.record(extra = mapOf(TestEventNumberKeys.TestEventNumber to "$i"))
            }

            assertTrue(click.testHasValue())

            // Trigger worker task to upload the pings in the background
            triggerWorkManager()

            val request = server.takeRequest(20L, TimeUnit.SECONDS)
            val applicationId = "mozilla-components-service-glean"
            assert(request.path.startsWith("/submit/$applicationId/events/${Glean.SCHEMA_VERSION}/"))
            val eventsJsonData = request.body.readUtf8()
            val eventsJson = checkPingSchema(eventsJsonData)
            val eventsArray = eventsJson.getJSONArray("events")
            assertEquals(500, eventsArray.length())

            for (i in 0..499) {
                assertEquals("$i", eventsArray.getJSONObject(i).getJSONObject("extra")["test_event_number"])
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
    fun `'extra' keys must be recorded and truncated if needed`() {
        val testEvent = EventMetricType<TruncatedKeys>(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "testEvent",
            sendInPings = listOf("store1"),
            allowedExtraKeys = listOf("extra1", "truncatedExtra")
        )

        val testValue = "LeanGleanByFrank"
        testEvent.record(
            extra = mapOf(
                TruncatedKeys.Extra1 to testValue,
                TruncatedKeys.TruncatedExtra to testValue.repeat(10)
            )
        )

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

    @Test
    fun `flush queued events on startup`() {
        assertEquals(
            "There should be no events on disk to start",
            0,
            EventsStorageEngine.storageDirectory.listFiles()?.size
        )

        val server = getMockWebServer()

        resetGlean(getContextWithMockedInfo(), Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        val event = EventMetricType<SomeExtraKeys>(
            disabled = false,
            category = "telemetry",
            name = "test_event",
            lifetime = Lifetime.Ping,
            sendInPings = listOf("events"),
            allowedExtraKeys = listOf("someExtra")
        )

        event.record(extra = mapOf(SomeExtraKeys.SomeExtra to "bar"))
        assertEquals(1, event.testGetValue().size)

        // Clear the in-memory storage only to mock being loaded in a fresh process
        EventsStorageEngine.eventStores.clear()
        resetGlean(
            getContextWithMockedInfo(),
            Glean.configuration.copy(
                serverEndpoint = "http://" + server.hostName + ":" + server.port,
                logPings = true
            ),
            clearStores = false
        )

        // Trigger worker task to upload the pings in the background
        triggerWorkManager()

        val request = server.takeRequest(20L, TimeUnit.SECONDS)
        assertEquals("POST", request.method)
        val applicationId = "mozilla-components-service-glean"
        assert(
            request.path.startsWith("/submit/$applicationId/events/${Glean.SCHEMA_VERSION}/")
        )
        val pingJsonData = request.body.readUtf8()
        val pingJson = JSONObject(pingJsonData)
        checkPingSchema(pingJson)
        assertNotNull(pingJson.opt("events"))
        assertEquals(
            1,
            pingJson.getJSONArray("events").length()
        )
    }

    @kotlinx.coroutines.ObsoleteCoroutinesApi
    @Test
    fun `flush queued events on startup and correctly handle pre-init events`() {
        assertEquals(
            "There should be no events on disk to start",
            0,
            EventsStorageEngine.storageDirectory.listFiles()?.size
        )

        val server = getMockWebServer()

        resetGlean(getContextWithMockedInfo(), Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        val event = EventMetricType<SomeExtraKeys>(
            disabled = false,
            category = "telemetry",
            name = "test_event",
            lifetime = Lifetime.Ping,
            sendInPings = listOf("events"),
            allowedExtraKeys = listOf("someExtra")
        )

        event.record(extra = mapOf(SomeExtraKeys.SomeExtra to "run1"))
        assertEquals(1, event.testGetValue().size)

        // Clear the in-memory storage only to mock being loaded in a fresh process
        EventsStorageEngine.eventStores.clear()

        Dispatchers.API.setTaskQueueing(true)

        event.record(extra = mapOf(SomeExtraKeys.SomeExtra to "pre-init"))

        resetGlean(
            getContextWithMockedInfo(),
            Glean.configuration.copy(
                serverEndpoint = "http://" + server.hostName + ":" + server.port,
                logPings = true
            ),
            clearStores = false
        )

        event.record(extra = mapOf(SomeExtraKeys.SomeExtra to "post-init"))

        // Trigger worker task to upload the pings in the background
        triggerWorkManager()

        var request = server.takeRequest(20L, TimeUnit.SECONDS)
        var pingJsonData = request.body.readUtf8()
        var pingJson = JSONObject(pingJsonData)
        checkPingSchema(pingJson)
        assertNotNull(pingJson.opt("events"))

        // This event comes from disk from the prior "run"
        assertEquals(
            1,
            pingJson.getJSONArray("events").length()
        )
        assertEquals(
            "run1",
            pingJson.getJSONArray("events").getJSONObject(0).getJSONObject("extra").getString("someExtra")
        )

        Glean.sendPingsByName(listOf("events"))

        // Trigger worker task to upload the pings in the background
        triggerWorkManager()

        request = server.takeRequest(20L, TimeUnit.SECONDS)
        pingJsonData = request.body.readUtf8()
        pingJson = JSONObject(pingJsonData)
        checkPingSchema(pingJson)
        assertNotNull(pingJson.opt("events"))

        // This event comes from the pre-initialization event
        assertEquals(
            2,
            pingJson.getJSONArray("events").length()
        )
        assertEquals(
            "pre-init",
            pingJson.getJSONArray("events").getJSONObject(0).getJSONObject("extra").getString("someExtra")
        )
        assertEquals(
            "post-init",
            pingJson.getJSONArray("events").getJSONObject(1).getJSONObject("extra").getString("someExtra")
        )
    }

    @Test
    fun `handle truncated events on disk`() {
        val server = getMockWebServer()

        resetGlean(getContextWithMockedInfo(), Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        val event = EventMetricType<ExtraKeys>(
            disabled = false,
            category = "telemetry",
            name = "test_event",
            lifetime = Lifetime.Ping,
            sendInPings = listOf("events"),
            allowedExtraKeys = listOf("key1", "key2")
        )

        // Record the event in the store, without providing optional arguments.
        event.record(mapOf(ExtraKeys.Key1 to "bar"))
        EventsStorageEngine.testWaitForWrites()
        // Add a couple of truncated events to disk. One is still valid JSON, the other isn't.
        // These event should be skipped, all others intact.
        EventsStorageEngine.writeEventToDisk("events", "[500]")
        EventsStorageEngine.testWaitForWrites()
        EventsStorageEngine.writeEventToDisk("events", "[500, \"foo")
        EventsStorageEngine.testWaitForWrites()
        event.record(mapOf(ExtraKeys.Key1 to "baz"))
        EventsStorageEngine.testWaitForWrites()
        assertEquals(2, event.testGetValue().size)

        // Clear the in-memory storage only to mock being loaded in a fresh process
        EventsStorageEngine.eventStores.clear()
        resetGlean(
            getContextWithMockedInfo(),
            Glean.configuration.copy(
                serverEndpoint = "http://" + server.hostName + ":" + server.port,
                logPings = true
            ),
            clearStores = false
        )
        triggerWorkManager()

        event.record(mapOf(ExtraKeys.Key1 to "bip"))

        val request = server.takeRequest(20L, TimeUnit.SECONDS)
        assertEquals("POST", request.method)
        val applicationId = "mozilla-components-service-glean"
        assert(
            request.path.startsWith("/submit/$applicationId/events/${Glean.SCHEMA_VERSION}/")
        )
        val pingJsonData = request.body.readUtf8()
        val pingJson = JSONObject(pingJsonData)
        checkPingSchema(pingJson)
        assertNotNull(pingJson.opt("events"))
        val events = pingJson.getJSONArray("events")
        assertEquals(2, events.length())
        assertEquals("bar", events.getJSONObject(0).getJSONObject("extra").getString("key1"))
        assertEquals("baz", events.getJSONObject(1).getJSONObject("extra").getString("key1"))
    }
}

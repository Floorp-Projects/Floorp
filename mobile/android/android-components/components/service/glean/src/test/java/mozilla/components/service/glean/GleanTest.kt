/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.service.glean.GleanMetrics.GleanInternalMetrics
import mozilla.components.service.glean.GleanMetrics.Pings
import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.private.CounterMetricType
import mozilla.components.service.glean.private.DatetimeMetricType
import mozilla.components.service.glean.private.EventMetricType
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.NoExtraKeys
import mozilla.components.service.glean.private.PingType
import mozilla.components.service.glean.private.StringMetricType
import mozilla.components.service.glean.private.UuidMetricType
import mozilla.components.service.glean.scheduler.GleanLifecycleObserver
import mozilla.components.service.glean.scheduler.PingUploadWorker
import mozilla.components.service.glean.storages.StorageEngineManager
import mozilla.components.service.glean.storages.StringsStorageEngine
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.service.glean.utils.getLanguageFromLocale
import mozilla.components.service.glean.utils.getLocaleTag
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.robolectric.RobolectricTestRunner
import java.io.BufferedReader
import java.io.File
import java.io.FileReader
import java.time.Instant
import java.util.Date
import java.util.Locale
import java.util.UUID
import java.util.concurrent.TimeUnit
import mozilla.components.service.glean.private.TimeUnit as GleanTimeUnit

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class GleanTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `disabling upload should disable metrics recording`() {
        val stringMetric = StringMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.Application,
                name = "string_metric",
                sendInPings = listOf("store1")
        )
        Glean.setUploadEnabled(false)
        assertEquals(false, Glean.getUploadEnabled())
        stringMetric.set("foo")
        assertNull(
                "Metrics should not be recorded if Glean is disabled",
                StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        )
    }

    @Test
    fun `test path generation`() {
        val uuid = UUID.randomUUID()
        val path = Glean.makePath("test", uuid)
        val applicationId = "mozilla-components-service-glean"
        // Make sure that the default applicationId matches the package name.
        assertEquals(applicationId, Glean.applicationId)
        assertEquals(path, "/submit/$applicationId/test/${Glean.SCHEMA_VERSION}/$uuid")
    }

    @Test
    fun `test experiments recording`() {
        Glean.setExperimentActive(
            "experiment_test", "branch_a"
        )
        Glean.setExperimentActive(
            "experiment_api", "branch_b",
            mapOf("test_key" to "value")
        )
        assertTrue(Glean.testIsExperimentActive("experiment_api"))
        assertTrue(Glean.testIsExperimentActive("experiment_test"))

        Glean.setExperimentInactive("experiment_test")

        assertTrue(Glean.testIsExperimentActive("experiment_api"))
        assertFalse(Glean.testIsExperimentActive("experiment_test"))

        val storedData = Glean.testGetExperimentData("experiment_api")
        assertEquals("branch_b", storedData.branch)
        assertEquals(1, storedData.extra?.size)
        assertEquals("value", storedData.extra?.getValue("test_key"))
    }

    @Test
    fun `test sending of background pings`() {
        val server = getMockWebServer()

        val click = EventMetricType<NoExtraKeys>(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "click",
            sendInPings = listOf("events")
        )

        resetGlean(getContextWithMockedInfo(), Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        // Fake calling the lifecycle observer.
        val lifecycleRegistry = LifecycleRegistry(mock(LifecycleOwner::class.java))
        val gleanLifecycleObserver = GleanLifecycleObserver()
        lifecycleRegistry.addObserver(gleanLifecycleObserver)

        try {
            // Simulate the first foreground event after the application starts.
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_START)
            click.record()

            // Simulate going to background.
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)

            // Trigger worker task to upload the pings in the background
            triggerWorkManager()

            val requests = mutableMapOf<String, String>()
            for (i in 0..1) {
                val request = server.takeRequest(20L, TimeUnit.SECONDS)
                val docType = request.path.split("/")[3]
                requests[docType] = request.body.readUtf8()
            }

            val eventsJson = JSONObject(requests["events"]!!)
            checkPingSchema(eventsJson)
            assertEquals("events", eventsJson.getJSONObject("ping_info")["ping_type"])
            assertEquals(1, eventsJson.getJSONArray("events").length())

            val baselineJson = JSONObject(requests["baseline"]!!)
            assertEquals("baseline", baselineJson.getJSONObject("ping_info")["ping_type"])
            checkPingSchema(baselineJson)

            val baselineMetricsObject = baselineJson.getJSONObject("metrics")
            val baselineStringMetrics = baselineMetricsObject.getJSONObject("string")
            assertEquals(1, baselineStringMetrics.length())
            assertNotNull(baselineStringMetrics.get("glean.baseline.locale"))

            val baselineTimespanMetrics = baselineMetricsObject.getJSONObject("timespan")
            assertEquals(1, baselineTimespanMetrics.length())
            assertNotNull(baselineTimespanMetrics.get("glean.baseline.duration"))
        } finally {
            server.shutdown()
            lifecycleRegistry.removeObserver(gleanLifecycleObserver)
        }
    }

    @Test
    fun `initialize() must not crash the app if Glean's data dir is messed up`() {
        // Remove the Glean's data directory.
        val gleanDir = File(
            ApplicationProvider.getApplicationContext<Context>().applicationInfo.dataDir,
            Glean.GLEAN_DATA_DIR
        )
        assertTrue(gleanDir.deleteRecursively())

        // Create a file in its place.
        assertTrue(gleanDir.createNewFile())

        resetGlean()

        // Clean up after this, so that other tests don't fail.
        assertTrue(gleanDir.delete())
    }

    @Test
    fun `Don't send metrics if not initialized`() {
        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_metric",
            sendInPings = listOf("store1")
        )
        Glean.initialized = false
        Dispatchers.API.testingMode = false
        stringMetric.set("foo")
        assertNull(
            "Metrics should not be recorded if Glean is not initialized",
            StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        )

        Glean.initialized = true
    }

    @Test
    fun `queued recorded metrics correctly record during init`() {
        val counterMetric = CounterMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "counter_metric",
            sendInPings = listOf("store1")
        )

        // First set to a pre-init state
        Glean.initialized = false
        Dispatchers.API.setTaskQueueing(true)

        // This will queue 3 tasks that will add to the metric value once Glean is initialized
        for (i in 0..2) {
            counterMetric.add()
        }

        // Ensure that no value has been stored yet since the tasks have only been queued and not
        // executed yet
        assertFalse("No value must be stored", counterMetric.testHasValue())

        // Calling resetGlean here will cause Glean to be initialized and should cause the queued
        // tasks recording metrics to execute
        resetGlean(clearStores = false)

        // Verify that the callback was executed by testing for the correct value
        assertTrue("Value must exist", counterMetric.testHasValue())
        assertEquals("Value must match", 3, counterMetric.testGetValue())
    }

    @Test
    fun `Initializing twice is a no-op`() {
        val beforeConfig = Glean.configuration

        Glean.initialize(ApplicationProvider.getApplicationContext())

        val afterConfig = Glean.configuration

        assertSame(beforeConfig, afterConfig)
    }

    @Test
    fun `Don't handle events when uninitialized`() {
        val gleanSpy = spy<GleanInternalAPI>(GleanInternalAPI::class.java)

        gleanSpy.initialized = false
        runBlocking {
            gleanSpy.handleBackgroundEvent()
        }
        assertFalse(getWorkerStatus(PingUploadWorker.PING_WORKER_TAG).isEnqueued)
    }

    @Test
    fun `Don't schedule pings if metrics disabled`() {
        Glean.setUploadEnabled(false)

        runBlocking {
            Glean.handleBackgroundEvent()
        }
        assertFalse(getWorkerStatus(PingUploadWorker.PING_WORKER_TAG).isEnqueued)
    }

    @Test
    fun `Don't schedule pings if there is no ping content`() {
        resetGlean(getContextWithMockedInfo())

        runBlocking {
            Glean.handleBackgroundEvent()
        }

        // We should only have a baseline ping and no events or metrics pings since nothing was
        // recorded
        val files = Glean.pingStorageEngine.storageDirectory.listFiles()!!

        // Make sure only the baseline ping is present and no events or metrics pings
        assertEquals(1, files.count())
        val file = files.first()
        BufferedReader(FileReader(file)).use {
            val lines = it.readLines()
            assert(lines[0].contains("baseline"))
        }
    }

    @Test
    fun `Application id sanitizer must correctly filter undesired characters`() {
        assertEquals(
            "org-mozilla-test-app",
            Glean.sanitizeApplicationId("org.mozilla.test-app")
        )

        assertEquals(
            "org-mozilla-test-app",
            Glean.sanitizeApplicationId("org.mozilla..test---app")
        )

        assertEquals(
            "org-mozilla-test-app",
            Glean.sanitizeApplicationId("org-mozilla-test-app")
        )
    }

    @Test
    fun `The appChannel must be correctly set, if requested`() {
        // No appChannel must be set if nothing was provided through the config
        // options.
        resetGlean(getContextWithMockedInfo(), Configuration())
        assertFalse(GleanInternalMetrics.appChannel.testHasValue())

        // The appChannel must be correctly reported if a channel value
        // was provided.
        val testChannelName = "my-test-channel"
        resetGlean(getContextWithMockedInfo(), Configuration(channel = testChannelName))
        assertTrue(GleanInternalMetrics.appChannel.testHasValue())
        assertEquals(testChannelName, GleanInternalMetrics.appChannel.testGetValue())
    }

    @Test
    fun `client_id and first_run_date metrics should be copied from the old location`() {
        // 1539480 BACKWARD COMPATIBILITY HACK

        // The resetGlean called right before this function will add client_id
        // and first_run_date to the new location in glean_client_info.  We
        // need to clear those out again so we can test what happens when they
        // are missing.
        StorageEngineManager(
            applicationContext = ApplicationProvider.getApplicationContext()
        ).clearAllStores()

        val clientIdMetric = UuidMetricType(
            disabled = false,
            category = "",
            name = "client_id",
            lifetime = Lifetime.User,
            sendInPings = listOf("glean_ping_info")
        )
        val clientIdValue = clientIdMetric.generateAndSet()

        val firstRunDateMetric = DatetimeMetricType(
            disabled = false,
            category = "",
            name = "first_run_date",
            lifetime = Lifetime.User,
            sendInPings = listOf("glean_ping_info"),
            timeUnit = GleanTimeUnit.Day
        )
        firstRunDateMetric.set()

        assertFalse(GleanInternalMetrics.clientId.testHasValue())
        assertFalse(GleanInternalMetrics.firstRunDate.testHasValue())

        // This should copy the values to their new locations
        Glean.initialized = false
        Glean.initialize(ApplicationProvider.getApplicationContext())

        assertEquals(clientIdValue, GleanInternalMetrics.clientId.testGetValue())
        assertTrue(GleanInternalMetrics.firstRunDate.testHasValue())
    }

    @Test
    fun `client_id and first_run_date metrics should not override new location`() {
        // 1539480 BACKWARD COMPATIBILITY HACK

        // The resetGlean called right before this function will add client_id
        // and first_run_date to the new location in glean_client_info.
        // In this case we want to keep those and confirm that any old values
        // won't override the new ones.

        val clientIdMetric = UuidMetricType(
            disabled = false,
            category = "",
            name = "client_id",
            lifetime = Lifetime.User,
            sendInPings = listOf("glean_ping_info")
        )
        val clientIdValue = clientIdMetric.generateAndSet()

        val firstRunDateMetric = DatetimeMetricType(
            disabled = false,
            category = "",
            name = "first_run_date",
            lifetime = Lifetime.User,
            sendInPings = listOf("glean_ping_info"),
            timeUnit = GleanTimeUnit.Day
        )
        firstRunDateMetric.set(Date.from(
            Instant.parse("2200-01-01T00:00:00.00Z")))

        assertTrue(GleanInternalMetrics.clientId.testHasValue())
        assertTrue(GleanInternalMetrics.firstRunDate.testHasValue())

        // This should copy the values to their new locations
        Glean.initialized = false
        Glean.initialize(ApplicationProvider.getApplicationContext())

        assertNotEquals(clientIdValue, GleanInternalMetrics.clientId.testGetValue())
        assertNotEquals(firstRunDateMetric.testGetValue(), GleanInternalMetrics.firstRunDate.testGetValue())
    }

    @Test
    fun `client_id and first_run_date must be generated if not available after the first start`() {
        // 1539480 BACKWARD COMPATIBILITY HACK

        // The resetGlean called right before this function will add client_id
        // and first_run_date to the new location in glean_client_info.  We
        // need to clear those out again so we can test what happens when they
        // are missing.
        StorageEngineManager(
            applicationContext = ApplicationProvider.getApplicationContext()
        ).clearAllStores()

        assertFalse(GleanInternalMetrics.clientId.testHasValue())
        assertFalse(GleanInternalMetrics.firstRunDate.testHasValue())

        // This should copy the values to their new locations
        Glean.initialized = false
        Glean.initialize(ApplicationProvider.getApplicationContext())

        assertTrue(GleanInternalMetrics.clientId.testHasValue())
        assertTrue(GleanInternalMetrics.firstRunDate.testHasValue())
    }

    @Test
    fun `getLanguageTag() reports the tag for the default locale`() {
        val defaultLanguageTag = getLocaleTag()

        assertNotNull(defaultLanguageTag)
        assertFalse(defaultLanguageTag.isEmpty())
        assertEquals("en-US", defaultLanguageTag)
    }

    @Test
    fun `getLanguageTag reports the correct tag for a non-default language`() {
        val defaultLocale = Locale.getDefault()

        try {
            Locale.setDefault(Locale("fy", "NL"))

            val languageTag = getLocaleTag()

            assertNotNull(languageTag)
            assertFalse(languageTag.isEmpty())
            assertEquals("fy-NL", languageTag)
        } finally {
            Locale.setDefault(defaultLocale)
        }
    }

    @Test
    fun `getLanguage reports the modern translation for some languages`() {
        assertEquals("he", getLanguageFromLocale(Locale("iw", "IL")))
        assertEquals("id", getLanguageFromLocale(Locale("in", "ID")))
        assertEquals("yi", getLanguageFromLocale(Locale("ji", "ID")))
    }

    @Test
    fun `ping collection must happen after currently scheduled metrics recordings`() {
        // Given the following block of code:
        //
        // Metric.A.set("SomeTestValue")
        // Glean.sendPings(listOf("custom-ping-1"))
        //
        // This test ensures that "custom-ping-1" contains "metric.a" with a value of "SomeTestValue"
        // when the ping is collected.

        val server = getMockWebServer()

        val pingName = "custom_ping_1"
        val ping = PingType(
            name = pingName,
            includeClientId = true
        )
        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_metric",
            sendInPings = listOf(pingName)
        )

        resetGlean(getContextWithMockedInfo(), Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        // This test relies on testing mode to be disabled, since we need to prove the
        // real-world async behaviour of this. We don't need to care about clearing it,
        // the test-unit hooks will call `resetGlean` anyway.
        Dispatchers.API.setTestingMode(false)

        // This is the important part of the test. Even though both the metrics API and
        // sendPings are async and off the main thread, "SomeTestValue" should be recorded,
        // the order of the calls must be preserved.
        val testValue = "SomeTestValue"
        stringMetric.set(testValue)
        ping.send()

        // Trigger worker task to upload the pings in the background. We need
        // to wait for the work to be enqueued first, since this test runs
        // asynchronously.
        waitForEnqueuedWorker(PingUploadWorker.PING_WORKER_TAG)
        triggerWorkManager()

        // Validate the received data.
        val request = server.takeRequest(20L, TimeUnit.SECONDS)
        val docType = request.path.split("/")[3]
        assertEquals(pingName, docType)

        val pingJson = JSONObject(request.body.readUtf8())
        assertEquals(pingName, pingJson.getJSONObject("ping_info")["ping_type"])
        checkPingSchema(pingJson)

        val pingMetricsObject = pingJson.getJSONObject("metrics")
        val pingStringMetrics = pingMetricsObject.getJSONObject("string")
        assertEquals(1, pingStringMetrics.length())
        assertEquals(testValue, pingStringMetrics.get("telemetry.string_metric"))
    }

    @Test
    fun `Basic metrics should be cleared when disabling uploading`() {
        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_metric",
            sendInPings = listOf("default")
        )

        stringMetric.set("TEST VALUE")
        assertTrue(stringMetric.testHasValue())

        Glean.setUploadEnabled(false)
        assertFalse(stringMetric.testHasValue())
        stringMetric.set("TEST VALUE")
        assertFalse(stringMetric.testHasValue())

        Glean.setUploadEnabled(true)
        assertFalse(stringMetric.testHasValue())
        stringMetric.set("TEST VALUE")
        assertTrue(stringMetric.testHasValue())
    }

    @Test
    fun `Core metrics should be cleared and restored when disabling and enabling uploading`() {
        assertTrue(GleanInternalMetrics.os.testHasValue())

        // Call this a couple of times to make sure it moves to a non-zero value
        Glean.pingMaker.getPingSeq("custom")
        assertTrue(Glean.pingMaker.getPingSeq("custom") > 0)

        Glean.setUploadEnabled(false)
        assertFalse(GleanInternalMetrics.os.testHasValue())
        assertEquals(0, Glean.pingMaker.getPingSeq("custom"))

        Glean.setUploadEnabled(true)
        assertTrue(GleanInternalMetrics.os.testHasValue())
    }

    @Test
    fun `firstRunDate is managed correctly when disabling and enabling metrics`() {
        val originalFirstRunDate = GleanInternalMetrics.firstRunDate.testGetValue()

        Glean.setUploadEnabled(false)
        assertEquals(originalFirstRunDate, GleanInternalMetrics.firstRunDate.testGetValue())

        Glean.setUploadEnabled(true)
        assertEquals(originalFirstRunDate, GleanInternalMetrics.firstRunDate.testGetValue())
    }

    @Test
    fun `disabling upload clears pending pings`() {
        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "string_metric",
            sendInPings = listOf("default")
        )

        stringMetric.set("TEST VALUE")

        Pings.baseline.send()
        assertEquals(1, Glean.pingStorageEngine.testGetNumPendingPings())

        Glean.setUploadEnabled(false)
        assertEquals(0, Glean.pingStorageEngine.testGetNumPendingPings())

        Glean.setUploadEnabled(true)
        assertEquals(0, Glean.pingStorageEngine.testGetNumPendingPings())
    }

    fun `clientId is managed correctly when disabling and enabling metrics`() {
        val originalClientId = GleanInternalMetrics.clientId.testGetValue()
        assertNotEquals(GleanInternalAPI.KNOWN_CLIENT_ID, GleanInternalMetrics.clientId.testGetValue())

        Glean.setUploadEnabled(false)
        assertEquals(GleanInternalAPI.KNOWN_CLIENT_ID, GleanInternalMetrics.clientId.testGetValue())

        Glean.setUploadEnabled(true)
        assertNotEquals(GleanInternalAPI.KNOWN_CLIENT_ID, GleanInternalMetrics.clientId.testGetValue())
        assertNotEquals(originalClientId, GleanInternalMetrics.clientId.testGetValue())
    }

    @Test
    fun `clientId is set to known id when setting disabled from a cold start`() {
        // Recreate "cold start" values
        Glean.initialized = false
        Glean.uploadEnabled = true

        Glean.setUploadEnabled(false)
        Glean.initialize(ApplicationProvider.getApplicationContext())

        assertEquals(GleanInternalAPI.KNOWN_CLIENT_ID, GleanInternalMetrics.clientId.testGetValue())
    }

    @Test
    fun `clientId is set to random id when setting enabled from a cold start`() {
        // Recreate "cold start" values
        Glean.initialized = false
        Glean.uploadEnabled = true

        Glean.setUploadEnabled(true)
        Glean.initialize(ApplicationProvider.getApplicationContext())

        assertNotEquals(GleanInternalAPI.KNOWN_CLIENT_ID, GleanInternalMetrics.clientId.testGetValue())
    }

    @Test
    fun `calling setUploadEnabled is a no-op`() {
        val gleanMock = mock(GleanInternalAPI::class.java)
        val context: Context = ApplicationProvider.getApplicationContext()

        gleanMock.initialize(context)
        gleanMock.setUploadEnabled(true)

        `when`(gleanMock.onChangeUploadEnabled(anyBoolean())).thenThrow(AssertionError::class.java)
        gleanMock.setUploadEnabled(true)
    }
}

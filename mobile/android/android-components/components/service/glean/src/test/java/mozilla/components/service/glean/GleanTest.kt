/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleOwner
import android.arch.lifecycle.LifecycleRegistry
import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.work.testing.WorkManagerTestInitHelper
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.service.glean.storages.StringsStorageEngine
import mozilla.components.service.glean.scheduler.GleanLifecycleObserver
import mozilla.components.service.glean.scheduler.MetricsPingScheduler
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.robolectric.RobolectricTestRunner
import java.io.BufferedReader
import java.io.File
import java.io.FileReader
import java.util.UUID
import java.util.concurrent.TimeUnit

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class GleanTest {

    @get:Rule
    val fakeDispatchers = FakeDispatchersInTest()

    @Before
    fun setup() {
        resetGlean()

        WorkManagerTestInitHelper.initializeTestWorkManager(
            ApplicationProvider.getApplicationContext())
    }

    @After
    fun resetGlobalState() {
        Glean.setUploadEnabled(true)
    }

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
                "Metrics should not be recorded if glean is disabled",
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
    fun `test sending of default pings`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_metric",
            sendInPings = listOf("default")
        )

        resetGlean(getContextWithMockedInfo(), Glean.configuration.copy(
            serverEndpoint = "http://" + server.hostName + ":" + server.port,
            logPings = true
        ))

        try {
            stringMetric.set("foo")

            Glean.setExperimentActive(
                "experiment1", "branch_a"
            )
            Glean.setExperimentActive(
                "experiment2", "branch_b",
                mapOf("key" to "value")
            )
            Glean.setExperimentInactive("experiment1")

            assertTrue(Glean.testIsExperimentActive("experiment2"))
            assertFalse(Glean.testIsExperimentActive("experiment1"))

            Glean.metricsPingScheduler.clearSchedulerData()
            Glean.handleEvent(Glean.PingEvent.Default)

            // Trigger worker task to upload the pings in the background
            triggerWorkManager()

            val request = server.takeRequest(20L, TimeUnit.SECONDS)
            assertEquals("POST", request.method)
            val metricsJsonData = request.body.readUtf8()
            val metricsJson = JSONObject(metricsJsonData)
            checkPingSchema(metricsJson)
            assertEquals(
                "foo",
                metricsJson.getJSONObject("metrics")
                    .getJSONObject("string")
                    .getString("telemetry.string_metric")
            )
            assertNull(metricsJson.opt("events"))
            assertNotNull(metricsJson.opt("ping_info"))
            assertNotNull(metricsJson.getJSONObject("ping_info").opt("experiments"))
            val applicationId = "mozilla-components-service-glean"
            assert(
                request.path.startsWith("/submit/$applicationId/metrics/${Glean.SCHEMA_VERSION}/")
            )
        } finally {
            server.shutdown()
        }
    }

    @Test
    fun `test sending of background pings`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

        val click = EventMetricType(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "click",
            sendInPings = listOf("default")
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
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)
            click.record()

            // Simulate going to background.
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)

            // Trigger worker task to upload the pings in the background
            triggerWorkManager()

            val requests: MutableMap<String, String> = mutableMapOf()
            for (i in 0..1) {
                val request = server.takeRequest(20L, TimeUnit.SECONDS)
                val docType = request.path.split("/")[3]
                requests.set(docType, request.body.readUtf8())
            }

            val eventsJson = JSONObject(requests["events"])
            checkPingSchema(eventsJson)
            assertEquals(1, eventsJson.getJSONArray("events")!!.length())

            val baselineJson = JSONObject(requests["baseline"])
            checkPingSchema(baselineJson)

            val expectedBaselineStringMetrics = arrayOf(
                "glean.baseline.os",
                "glean.baseline.os_version",
                "glean.baseline.device_manufacturer",
                "glean.baseline.device_model",
                "glean.baseline.architecture",
                "glean.baseline.locale"
            )
            val baselineMetricsObject = baselineJson.getJSONObject("metrics")!!
            val baselineStringMetrics = baselineMetricsObject.getJSONObject("string")!!
            assertEquals(expectedBaselineStringMetrics.size, baselineStringMetrics.length())
            for (metric in expectedBaselineStringMetrics) {
                assertNotNull(baselineStringMetrics.get(metric))
            }

            val baselineTimespanMetrics = baselineMetricsObject.getJSONObject("timespan")!!
            assertEquals(1, baselineTimespanMetrics.length())
            assertNotNull(baselineTimespanMetrics.get("glean.baseline.duration"))
        } finally {
            server.shutdown()
            lifecycleRegistry.removeObserver(gleanLifecycleObserver)
        }
    }

    @Test
    fun `test sending of metrics ping`() {
        val server = MockWebServer()
        server.enqueue(MockResponse().setBody("OK"))

        val metricToSend = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "metrics_ping",
            sendInPings = listOf("default")
        )

        val metricsPingScheduler = MetricsPingScheduler(getContextWithMockedInfo())
        metricsPingScheduler.clearSchedulerData()

        // Store a ping timestamp from 25 hours ago to compare to so that the ping will get sent
        metricsPingScheduler.updateSentTimestamp(
            System.currentTimeMillis() - TimeUnit.HOURS.toMillis(25))

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
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)

            // Write a value to the metric to check later
            metricToSend.set("Test metrics ping")

            // Simulate going to background so that the metrics ping schedule will be checked
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)

            // Trigger worker task to upload the pings in the background
            triggerWorkManager()

            val requests: MutableMap<String, String> = mutableMapOf()
            for (i in 0..1) {
                val request = server.takeRequest(20L, TimeUnit.SECONDS)
                val docType = request.path.split("/")[3]
                requests[docType] = request.body.readUtf8()
            }

            val metricsJson = JSONObject(requests["metrics"])
            checkPingSchema(metricsJson)

            // Check ping type
            val pingInfo = metricsJson.getJSONObject("ping_info")!!
            assertEquals(pingInfo["ping_type"], "metrics")

            // Retrieve the string metric that was stored
            val metricsObject = metricsJson.getJSONObject("metrics")!!
            val stringMetrics = metricsObject.getJSONObject("string")!!

            assertEquals(stringMetrics["telemetry.metrics_ping"], "Test metrics ping")
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
        stringMetric.set("foo")
        assertNull(
            "Metrics should not be recorded if glean is not initialized",
            StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        )

        Glean.initialized = true
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
        gleanSpy.handleEvent(Glean.PingEvent.Background)
        assertFalse(isWorkScheduled())
    }

    @Test
    fun `Don't schedule pings if metrics disabled`() {
        Glean.setUploadEnabled(false)

        Glean.handleEvent(Glean.PingEvent.Background)

        assertFalse(isWorkScheduled())
    }

    @Test
    fun `Don't schedule pings if there is no ping content`() {
        resetGlean(getContextWithMockedInfo())

        Glean.handleEvent(Glean.PingEvent.Background)

        // We should only have a baseline ping and no events or metrics pings since nothing was
        // recorded
        val files = Glean.pingStorageEngine.storageDirectory.listFiles()

        // Make sure only the baseline ping is present and no events or metrics pings
        assertEquals(1, files.count())
        val file = files.first()
        BufferedReader(FileReader(file)).use {
            val lines = it.readLines()
            assert(lines[0].contains("baseline"))
        }
    }

    @Test
    fun `Application id sanitazer must correctly filter undesired characters`() {
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
    fun `metricsPingScheduler is properly initialized`() {
        Glean.metricsPingScheduler.clearSchedulerData()
        Glean.metricsPingScheduler.updateSentTimestamp()

        // Should return false since we just updated the last time the ping was sent above
        assertFalse(Glean.metricsPingScheduler.canSendPing())
    }
}

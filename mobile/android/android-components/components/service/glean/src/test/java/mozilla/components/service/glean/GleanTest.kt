/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test

import java.util.UUID
import org.json.JSONObject

import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.mock
import org.mockito.Mockito.times

import mozilla.components.service.glean.config.Configuration
import mozilla.components.service.glean.net.HttpPingUploader
import mozilla.components.service.glean.storages.EventsStorageEngine
import mozilla.components.service.glean.storages.ExperimentsStorageEngine
import mozilla.components.service.glean.storages.StringsStorageEngine

import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class GleanTest {
    @Before
    fun setup() {
        Glean.initialize(
            applicationContext = RuntimeEnvironment.application,
            configuration = Configuration()
        )
    }

    @After
    fun resetGlobalState() {
        Glean.setMetricsEnabled(true)
        Glean.clearExperiments()
    }

    @Test
    fun `disabling metrics should record nothing`() {
        val stringMetric = StringMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.Application,
                name = "string_metric",
                sendInPings = listOf("store1")
        )
        StringsStorageEngine.clearAllStores()
        Glean.setMetricsEnabled(false)
        assertEquals(false, Glean.getMetricsEnabled())
        stringMetric.set("foo")
        assertNull(
                "Metrics should not be recorded if glean is disabled",
                StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        )
    }

    @Test
    fun `disabling event metrics should record only when enabled`() {
        val eventMetric = EventMetricType(
                disabled = false,
                category = "ui",
                lifetime = Lifetime.Ping,
                name = "event_metric",
                sendInPings = listOf("store1"),
                objects = listOf("buttonA")
        )
        EventsStorageEngine.clearAllStores()
        Glean.setMetricsEnabled(true)
        assertEquals(true, Glean.getMetricsEnabled())
        eventMetric.record("buttonA", "event1")
        val snapshot1 = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot1!!.size)
        Glean.setMetricsEnabled(false)
        assertEquals(false, Glean.getMetricsEnabled())
        eventMetric.record("buttonA", "event2")
        val snapshot2 = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot2!!.size)
        Glean.setMetricsEnabled(true)
        eventMetric.record("buttonA", "event3")
        val snapshot3 = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(2, snapshot3!!.size)
    }

    @Test
    fun `test path generation`() {
        val uuid = UUID.randomUUID()
        val path = Glean.makePath("test", uuid)
        assertEquals(path, "/submit/glean/test/${Glean.SCHEMA_VERSION}/$uuid")
    }

    @Test
    fun `test sending of default pings`() {
        val realClient = Glean.httpPingUploader
        val client = mock(HttpPingUploader::class.java)
        Glean.httpPingUploader = client
        StringsStorageEngine.clearAllStores()
        ExperimentsStorageEngine.clearAllStores()

        try {
            val payloads: MutableMap<String, Pair<String, String>> = mutableMapOf()

            doAnswer {
                val parts = (it.arguments[1] as String).split("/")
                payloads.set(parts[3], Pair(it.arguments[0] as String, it.arguments[1] as String))
                null
            }.`when`(client).upload(anyString(), anyString())

            val stringMetric = StringMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.Application,
                name = "string_metric",
                sendInPings = listOf("default")
            )
            stringMetric.set("foo")

            Glean.setExperimentActive(
                "experiment1", "branch_a"
            )
            Glean.setExperimentActive(
                "experiment2", "branch_b",
                    mapOf("key" to "value")
            )
            Glean.setExperimentInactive("experiment1")

            Glean.handleEvent(Glean.PingEvent.Default)
            assertEquals(1, payloads.size)

            val (metricsJsonData, metricsPath) = payloads.get("metrics")!!
            val metricsJson = JSONObject(metricsJsonData)
            assertEquals(
                "foo",
                metricsJson.getJSONObject("metrics")
                    .getJSONObject("string")
                    .getString("telemetry.string_metric")
            )
            assertNull(metricsJson.opt("events"))
            assertNotNull(metricsJson.opt("experiments"))
            assertNotNull(metricsJson.opt("ping_info"))
            assert(
                metricsPath.startsWith("/submit/glean/metrics/${Glean.SCHEMA_VERSION}/")
            )
        } finally {
            Glean.httpPingUploader = realClient
        }
    }
}

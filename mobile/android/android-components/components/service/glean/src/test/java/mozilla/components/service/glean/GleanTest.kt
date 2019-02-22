/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleOwner
import android.arch.lifecycle.LifecycleRegistry
import android.content.Context
import android.os.SystemClock
import android.util.Log
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.core.app.ApplicationProvider
import androidx.work.Configuration
import androidx.work.testing.SynchronousExecutor
import androidx.work.testing.WorkManagerTestInitHelper
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.service.glean.scheduler.MetricsPingScheduler
import mozilla.components.service.glean.scheduler.PingScheduler
import mozilla.components.service.glean.storages.StringsStorageEngine
import mozilla.components.service.glean.storages.EventsStorageEngine
import mozilla.components.service.glean.storages.StorageEngineManager
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner
import java.io.BufferedReader
import java.io.File
import java.io.FileReader
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

        val mTargetContext = InstrumentationRegistry.getInstrumentation().targetContext
        val mConfiguration = Configuration.Builder()
            .setExecutor(SynchronousExecutor())
            .setMinimumLoggingLevel(Log.DEBUG)
            .build()
        // Initialize WorkManager using the WorkManagerTestInitHelper.
        WorkManagerTestInitHelper.initializeTestWorkManager(mTargetContext, mConfiguration)
    }

    @After
    fun resetGlobalState() {
        Glean.setUploadEnabled(true)
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
        Glean.testClearAllData()
        Glean.setUploadEnabled(false)
        assertEquals(false, Glean.getUploadEnabled())
        stringMetric.set("foo")
        assertNull(
                "Metrics should not be recorded if glean is disabled",
                StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        )
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
        Glean.testClearAllData()
        Glean.initialized = false
        stringMetric.set("foo")
        assertNull(
            "Metrics should not be recorded if glean is not initialized",
            StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        )

        Glean.initialized = true
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
        Glean.testClearAllData()
        assertEquals(true, Glean.getUploadEnabled())
        Glean.setUploadEnabled(true)
        eventMetric.record("buttonA", "event1")
        val snapshot1 = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot1!!.size)
        Glean.setUploadEnabled(false)
        assertEquals(false, Glean.getUploadEnabled())
        eventMetric.record("buttonA", "event2")
        val snapshot2 = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot2!!.size)
        Glean.setUploadEnabled(true)
        eventMetric.record("buttonA", "event3")
        val snapshot3 = EventsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(2, snapshot3!!.size)
    }

    @Test
    fun `test scheduling of background pings`() {
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
            logPings = true
        ))

        // Fake calling the lifecycle observer.
        val lifecycleRegistry = LifecycleRegistry(mock(LifecycleOwner::class.java))
        val context: Context = ApplicationProvider.getApplicationContext()
        val pingScheduler = PingScheduler(context,
            StorageEngineManager(applicationContext = context))
        lifecycleRegistry.addObserver(pingScheduler)

        try {
            // Simulate the first foreground event after the application starts.
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)
            click.record("buttonA")

            // Simulate going to background.
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)

            // Make sure that the PingUploadWorker was scheduled
            assertTrue(isWorkScheduled())
        } finally {
            lifecycleRegistry.removeObserver(pingScheduler)
        }
    }

    @Test
    fun `test serialization of event ping when it fills up at 500 events`() {
        EventsStorageEngine.clearAllStores()
        val click = EventMetricType(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "click",
            sendInPings = listOf("events"),
            objects = listOf("buttonA")
        )

        // Make sure no PingUploadWorker is scheduled yet
        assertFalse(isWorkScheduled())

        // We send 510 events.  We expect to get the first 500 in the ping, and 10 remaining afterward
        for (i in 0..509) {
            click.record("buttonA", "$i")
            // Need a little bit of delay between clicks for sanity and to prevent a large number
            // of background tasks getting spawned all at once.
            Thread.sleep(5)
        }

        assertTrue("Storage operation didn't complete correctly", click.testHasValue())

        // Now check to see that there was an events ping serialized
        val pingFiles = Glean.pingScheduler.pingStorageEngine.storageDirectory.listFiles()
        assertEquals(1, pingFiles?.count())
        BufferedReader(FileReader(pingFiles?.first())).use {
            val path = it.readLine()
            assertTrue(path.contains("events"))
        }

        val remaining = EventsStorageEngine.getSnapshot("events", false)!!
        assertEquals(10, remaining.size)
        for (i in 0..9) {
            assertEquals("${i + 500}", remaining[i].value)
        }
    }

    @Test
    fun `test scheduling of metrics ping`() {
        StringsStorageEngine.clearAllStores()
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
            logPings = true
        ))

        // Fake calling the lifecycle observer.
        val lifecycleRegistry = LifecycleRegistry(mock(LifecycleOwner::class.java))
        val context: Context = ApplicationProvider.getApplicationContext()
        val pingScheduler = PingScheduler(context,
            StorageEngineManager(applicationContext = context))
        lifecycleRegistry.addObserver(pingScheduler)

        try {
            // Simulate the first foreground event after the application starts.
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)

            // Write a value to the metric to check later
            metricToSend.set("Test metrics ping")

            // Simulate going to background so that the metrics ping schedule will be checked
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)

            // Make sure the PingUploadWorker was scheduled
            assertTrue(isWorkScheduled())
        } finally {
            lifecycleRegistry.removeObserver(pingScheduler)
        }
    }

    @Test
    fun `Check serialized ping contents for valid ping data`() {
        EventsStorageEngine.clearAllStores()
        resetGlean(getContextWithMockedInfo())

        // Define a 'click' event for the purpose of validating events ping schema
        val click = EventMetricType(
            disabled = false,
            category = "ui",
            lifetime = Lifetime.Ping,
            name = "click",
            sendInPings = listOf("events"),
            objects = listOf("buttonA", "buttonB")
        )
        // Record two events of the same type, with a little delay.
        click.record("buttonA")
        val expectedTimeSinceStart: Long = 37
        SystemClock.sleep(expectedTimeSinceStart)
        click.record("buttonB")

        // Define a StringMetricType for validation of the metrics ping schema
        val stringMetric = StringMetricType(
            disabled = false,
            category = "test",
            lifetime = Lifetime.Ping,
            name = "test_string",
            sendInPings = listOf("metrics")
        )
        stringMetric.set("Glean Rules!")

        // Fake calling the lifecycle observer.
        val lifecycleRegistry = LifecycleRegistry(Mockito.mock(LifecycleOwner::class.java))
        val context: Context = ApplicationProvider.getApplicationContext()
        val pingScheduler = PingScheduler(context,
            StorageEngineManager(applicationContext = context))
        lifecycleRegistry.addObserver(pingScheduler)
        try {
            // Simulate the first foreground event after the application starts.
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)

            // Simulate going to background.
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)

            // Make sure the PingUploadWorker is scheduled
            assertTrue(isWorkScheduled())

            val pingList = pingScheduler.pingStorageEngine.storageDirectory.listFiles()

            // This should at contain a baseline, events, and a metrics ping
            pingList?.forEach { file ->
                val bufferedReader = BufferedReader(FileReader(file))
                val path = bufferedReader.readLine()
                val serializedPing = bufferedReader.readLine()

                // Compare the path UUID to the filename
                assertEquals(path.substring(path.lastIndexOf("/") + 1), file.name)
                // Validate the ping schema
                checkPingSchema(serializedPing)
            }
        } finally {
            lifecycleRegistry.removeObserver(pingScheduler)
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
    fun `Initializing twice is a no-op`() {
        val beforeConfig = Glean.configuration

        Glean.initialize(ApplicationProvider.getApplicationContext())

        val afterConfig = Glean.configuration

        assertSame(beforeConfig, afterConfig)
    }
}

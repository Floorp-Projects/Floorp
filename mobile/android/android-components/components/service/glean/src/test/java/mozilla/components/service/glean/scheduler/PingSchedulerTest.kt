/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.scheduler

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleOwner
import android.arch.lifecycle.LifecycleRegistry
import android.content.Context
import android.util.Log
import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import androidx.work.Configuration
import androidx.work.WorkManager
import androidx.work.testing.SynchronousExecutor
import androidx.work.testing.WorkManagerTestInitHelper
import mozilla.components.service.glean.resetGlean
import mozilla.components.service.glean.TestLifeCycleOwner
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.isWorkScheduled
import mozilla.components.service.glean.storages.EventsStorageEngine
import mozilla.components.service.glean.storages.StorageEngineManager
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers
import org.mockito.Mockito
import java.io.BufferedReader
import java.io.FileReader
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
class PingSchedulerTest {
    private lateinit var pingScheduler: PingScheduler
    private var workManager: WorkManager? = null
    private lateinit var lifeCycleOwner: LifecycleOwner
    private lateinit var targetContext: Context
    private lateinit var configuration: Configuration

    @Before
    fun setup() {
        resetGlean()

        pingScheduler = Glean.pingScheduler

        targetContext = InstrumentationRegistry.getInstrumentation().targetContext
        lifeCycleOwner = TestLifeCycleOwner()
        configuration = Configuration.Builder()
            .setExecutor(SynchronousExecutor())
            .setMinimumLoggingLevel(Log.DEBUG)
            .build()
        // Initialize WorkManager using the WorkManagerTestInitHelper.
        WorkManagerTestInitHelper.initializeTestWorkManager(targetContext, configuration)
        workManager = WorkManager.getInstance()

        // Clear out pings and assert that there are none in the directory before we start
        pingScheduler.pingStorageEngine.storageDirectory.listFiles()?.forEach { file ->
            file.delete()
        }
        Assert.assertNull("Pending pings directory must be empty before test start",
            pingScheduler.pingStorageEngine.storageDirectory.listFiles())
    }

    @Test
    fun `test path generation`() {
        val uuid = UUID.randomUUID()
        val path = pingScheduler.makePath("test", uuid)
        val applicationId = "mozilla-components-service-glean"
        // Make sure that the default applicationId matches the package name.
        assertEquals(applicationId, pingScheduler.applicationId)
        assertEquals(path, "/submit/$applicationId/test/${Glean.SCHEMA_VERSION}/$uuid")
    }

    @Test
    fun `Application id sanitizer must correctly filter undesired characters`() {
        assertEquals(
            "org-mozilla-test-app",
            pingScheduler.sanitizeApplicationId("org.mozilla.test-app")
        )

        assertEquals(
            "org-mozilla-test-app",
            pingScheduler.sanitizeApplicationId("org.mozilla..test---app")
        )

        assertEquals(
            "org-mozilla-test-app",
            pingScheduler.sanitizeApplicationId("org-mozilla-test-app")
        )
    }

    @Test
    fun `metricsPingScheduler is properly initialized`() {
        pingScheduler.metricsPingScheduler.clearSchedulerData()
        pingScheduler.metricsPingScheduler.updateSentTimestamp()

        // Should return false since we just updated the last time the ping was sent above
        assertFalse(pingScheduler.metricsPingScheduler.canSendPing())
    }

    @Test
    fun `Going to background doesn't schedule pings when uninitialized`() {
        val gleanSpy = Mockito.mock<PingScheduler>(PingScheduler::class.java)

        Mockito.doThrow(IllegalStateException("Shouldn't schedule ping")).`when`(gleanSpy)
            gleanSpy.assembleAndSerializePing(ArgumentMatchers.anyString())
        Glean.initialized = false

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

            // Ensure that the worker was not scheduled
            assertFalse(isWorkScheduled())
        } finally {
            lifecycleRegistry.removeObserver(pingScheduler)
        }
    }

    @Test
    fun `Don't schedule pings if metrics disabled`() {
        Glean.setUploadEnabled(false)

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

            // Ensure that the worker was not scheduled
            assertFalse(isWorkScheduled())
        } finally {
            lifecycleRegistry.removeObserver(pingScheduler)
        }
    }

    @Test
    fun `Don't schedule pings if there is no ping content`() {
        EventsStorageEngine.clearAllStores()

        // Simulate the first foreground event after the application starts.
        pingScheduler.onEnterForeground()

        // Simulate going to background.
        pingScheduler.onEnterBackground()

        // Get a list of files to determine that there are no "events" or "metrics" pings,
        // only the baseline ping since it should always be there.
        val files = pingScheduler.pingStorageEngine.storageDirectory.listFiles()

        assertEquals(1, files?.count())

        BufferedReader(FileReader(files?.first())).use {
            val path = it.readLine()
            assertTrue(path.contains("baseline"))
        }
    }
}

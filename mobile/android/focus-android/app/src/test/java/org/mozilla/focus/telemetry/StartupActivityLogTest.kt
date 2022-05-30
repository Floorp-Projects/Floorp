/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.app.Activity
import android.app.Application
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.mozilla.focus.telemetry.startuptelemetry.StartupActivityLog
import org.mozilla.focus.telemetry.startuptelemetry.StartupActivityLog.LogEntry

class StartupActivityLogTest {

    private lateinit var log: StartupActivityLog
    private lateinit var appObserver: StartupActivityLog.StartupLogAppLifecycleObserver
    private lateinit var activityCallbacks: StartupActivityLog.StartupLogActivityLifecycleCallbacks

    @Before
    fun setUp() {
        log = StartupActivityLog()
        val (appObserver, activityCallbacks) = log.getObserversForTesting()
        this.appObserver = appObserver
        this.activityCallbacks = activityCallbacks
    }

    @Test
    fun `WHEN register is called THEN it is registered`() {
        val app: Application = mock()
        val lifecycleOwner: LifecycleOwner = mock()
        val mLifecycle: Lifecycle = mock()
        `when`(lifecycleOwner.lifecycle).thenReturn(mLifecycle)

        log.registerInAppOnCreate(app, lifecycleOwner)

        verify(app).registerActivityLifecycleCallbacks(any())
    }

    @Test // we test start and stop individually due to the clear-on-stop behavior.
    fun `WHEN app observer start is called THEN it is added directly to the log`() {
        assertTrue(log.log.isEmpty())

        appObserver.onStart(mock())
        assertEquals(listOf(LogEntry.AppStarted), log.log)

        appObserver.onStart(mock())
        assertEquals(listOf(LogEntry.AppStarted, LogEntry.AppStarted), log.log)
    }

    @Test // we test start and stop individually due to the clear-on-stop behavior.
    fun `WHEN app observer stop is called THEN it is added directly to the log`() {
        assertTrue(log.log.isEmpty())

        appObserver.onStop(mock())
        assertEquals(listOf(LogEntry.AppStopped), log.log)
    }

    @Test
    fun `WHEN activity callback methods are called THEN they are added directly to the log`() {
        assertTrue(log.log.isEmpty())
        val expected = mutableListOf<LogEntry>()

        val activityClass = mock<Activity>()::class.java // mockk can't mock Class<...>

        activityCallbacks.onActivityCreated(mock(), null)
        expected.add(LogEntry.ActivityCreated(activityClass))
        assertEquals(expected, log.log)

        activityCallbacks.onActivityStarted(mock())
        expected.add(LogEntry.ActivityStarted(activityClass))
        assertEquals(expected, log.log)

        activityCallbacks.onActivityStopped(mock())
        expected.add(LogEntry.ActivityStopped(activityClass))
        assertEquals(expected, log.log)
    }

    @Test
    fun `WHEN app STOPPED is called THEN the log is emptied expect for the stop event`() {
        assertTrue(log.log.isEmpty())

        activityCallbacks.onActivityCreated(mock(), null)
        activityCallbacks.onActivityStarted(mock())
        appObserver.onStart(mock())
        assertEquals(3, log.log.size)

        appObserver.onStop(mock())
        assertEquals(listOf(LogEntry.AppStopped), log.log)
    }
}

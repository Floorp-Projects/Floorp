/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.Activity
import android.app.Application
import androidx.lifecycle.LifecycleOwner
import io.mockk.Called
import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.base.log.Log.Priority
import mozilla.components.support.base.log.logger.Logger
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.perf.StartupActivityLog.LogEntry

class StartupActivityLogTest {

    private lateinit var log: StartupActivityLog
    private lateinit var appObserver: StartupActivityLog.StartupLogAppLifecycleObserver
    private lateinit var activityCallbacks: StartupActivityLog.StartupLogActivityLifecycleCallbacks

    @MockK(relaxed = true)
    private lateinit var logger: Logger

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        log = StartupActivityLog()
        val (appObserver, activityCallbacks) = log.getObserversForTesting()
        this.appObserver = appObserver
        this.activityCallbacks = activityCallbacks
    }

    @Test
    fun `WHEN register is called THEN it is registered`() {
        val app = mockk<Application>(relaxed = true)
        val lifecycleOwner = mockk<LifecycleOwner>(relaxed = true)
        log.registerInAppOnCreate(app, lifecycleOwner)

        verify { app.registerActivityLifecycleCallbacks(any()) }
        verify { lifecycleOwner.lifecycle.addObserver(any()) }
    }

    @Test // we test start and stop individually due to the clear-on-stop behavior.
    fun `WHEN app observer start is called THEN it is added directly to the log`() {
        assertTrue(log.log.isEmpty())

        appObserver.onStart(mockk())
        assertEquals(listOf(LogEntry.AppStarted), log.log)

        appObserver.onStart(mockk())
        assertEquals(listOf(LogEntry.AppStarted, LogEntry.AppStarted), log.log)
    }

    @Test // we test start and stop individually due to the clear-on-stop behavior.
    fun `WHEN app observer stop is called THEN it is added directly to the log`() {
        assertTrue(log.log.isEmpty())

        appObserver.onStop(mockk())
        assertEquals(listOf(LogEntry.AppStopped), log.log)
    }

    @Test
    fun `WHEN activity callback methods are called THEN they are added directly to the log`() {
        assertTrue(log.log.isEmpty())
        val expected = mutableListOf<LogEntry>()

        val activityClass = mockk<Activity>()::class.java // mockk can't mock Class<...>

        activityCallbacks.onActivityCreated(mockk(), null)
        expected.add(LogEntry.ActivityCreated(activityClass))
        assertEquals(expected, log.log)

        activityCallbacks.onActivityStarted(mockk())
        expected.add(LogEntry.ActivityStarted(activityClass))
        assertEquals(expected, log.log)

        activityCallbacks.onActivityStopped(mockk())
        expected.add(LogEntry.ActivityStopped(activityClass))
        assertEquals(expected, log.log)
    }

    @Test
    fun `WHEN app STOPPED is called THEN the log is emptied expect for the stop event`() {
        assertTrue(log.log.isEmpty())

        activityCallbacks.onActivityCreated(mockk(), null)
        activityCallbacks.onActivityStarted(mockk())
        appObserver.onStart(mockk())
        assertEquals(3, log.log.size)

        appObserver.onStop(mockk())
        assertEquals(listOf(LogEntry.AppStopped), log.log)
    }

    @Test
    fun `GIVEN debug log level WHEN logEntries is called THEN there is no logcat call`() {
        log.logEntries(logger, Priority.DEBUG)
        verify { logger.debug(any()) }
    }

    @Test
    fun `GIVEN info log level WHEN logEntries is called THEN there is a logcat call`() {
        log.logEntries(logger, Priority.INFO)
        verify { logger wasNot Called }
    }
}

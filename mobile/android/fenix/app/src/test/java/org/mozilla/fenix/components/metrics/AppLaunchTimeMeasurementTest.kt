/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.os.SystemClock
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.HOT
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.COLD
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.ERROR
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.WARM
import org.mozilla.fenix.perf.Stat

class AppLaunchTimeMeasurementTest {

    @MockK
    private lateinit var statMock: Stat

    private lateinit var appLaunchTimeMeasurement: AppLaunchTimeMeasurement
    private val startTime = SystemClock.elapsedRealtimeNanos()
    private val endTime = SystemClock.elapsedRealtimeNanos() + 1

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
        appLaunchTimeMeasurement = AppLaunchTimeMeasurement(statMock)

        every { statMock.getProcessStartTimeStampNano(any()) } returns startTime
    }

    @Test
    fun `WHEN application is launched with cold startup THEN report the correct value`() {
        runBlocking {
            appLaunchTimeMeasurement.onFirstFramePreDraw(endTime)

            val actualResult = endTime.minus(startTime)
            assertTrue(appLaunchTimeMeasurement.getApplicationLaunchTime(COLD) == actualResult)
        }
    }

    @Test
    fun `WHEN application is launch with warm startup THEN report the correct value`() {
        appLaunchTimeMeasurement.onHomeActivityOnCreate(startTime)
        appLaunchTimeMeasurement.onFirstFramePreDraw(endTime)

        val actualResult = endTime.minus(startTime)
        runBlocking {
            assertTrue(appLaunchTimeMeasurement.getApplicationLaunchTime(WARM) == actualResult)
        }
    }

    @Test
    fun `WHEN application is launch with hot startup THEN report the correct value`() {
        appLaunchTimeMeasurement.onHomeActivityOnRestart(startTime)
        appLaunchTimeMeasurement.onFirstFramePreDraw(endTime)

        val actualResult = endTime.minus(startTime)
        runBlocking {
            assertTrue(appLaunchTimeMeasurement.getApplicationLaunchTime(HOT) == actualResult)
        }
    }

    @Test
    fun `WHEN getting launch time before onDraw() is called THEN report the correct value`() {
        appLaunchTimeMeasurement.onHomeActivityOnCreate(startTime)

        val actualResult = null
        runBlocking {
            assertTrue(appLaunchTimeMeasurement.getApplicationLaunchTime(ERROR) == actualResult)
        }
    }
}

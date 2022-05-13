/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import androidx.lifecycle.Lifecycle
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.ktx.kotlin.crossProduct
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.PerfStartup
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.perf.StartupPathProvider.StartupPath
import org.mozilla.fenix.perf.StartupStateProvider.StartupState

private val validTelemetryLabels = run {
    val allStates = listOf("cold", "warm", "hot", "unknown")
    val allPaths = listOf("main", "view", "unknown")

    allStates.crossProduct(allPaths) { state, path -> "${state}_$path" }.toSet()
}

private val activityClass = HomeActivity::class.java

@RunWith(FenixRobolectricTestRunner::class)
class StartupTypeTelemetryTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var telemetry: StartupTypeTelemetry
    private lateinit var callbacks: StartupTypeTelemetry.StartupTypeLifecycleObserver
    @MockK private lateinit var stateProvider: StartupStateProvider
    @MockK private lateinit var pathProvider: StartupPathProvider

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
        telemetry = spyk(StartupTypeTelemetry(stateProvider, pathProvider))
        callbacks = telemetry.getTestCallbacks()
    }

    @Test
    fun `WHEN attach is called THEN it is registered to the lifecycle`() {
        val lifecycle = mockk<Lifecycle>(relaxed = true)
        telemetry.attachOnHomeActivityOnCreate(lifecycle)

        verify { lifecycle.addObserver(any()) }
    }

    @Test
    fun `GIVEN all possible path and state combinations WHEN record telemetry THEN the labels are incremented the appropriate number of times`() {
        val allPossibleInputArgs = StartupState.values().toList().crossProduct(
            StartupPath.values().toList()
        ) { state, path ->
            Pair(state, path)
        }

        allPossibleInputArgs.forEach { (state, path) ->
            every { stateProvider.getStartupStateForStartedActivity(activityClass) } returns state
            every { pathProvider.startupPathForActivity } returns path

            telemetry.record()
        }

        validTelemetryLabels.forEach { label ->
            // Path == NOT_SET gets bucketed with Path == UNKNOWN so we'll increment twice for those.
            val expected = if (label.endsWith("unknown")) 2 else 1
            assertEquals("label: $label", expected, PerfStartup.startupType[label].testGetValue())
        }

        // All invalid labels go to a single bucket: let's verify it has no value.
        assertNull(PerfStartup.startupType["__other__"].testGetValue())
    }

    @Test
    fun `WHEN record is called THEN telemetry is recorded with the appropriate label`() {
        every { stateProvider.getStartupStateForStartedActivity(activityClass) } returns StartupState.COLD
        every { pathProvider.startupPathForActivity } returns StartupPath.MAIN

        telemetry.record()

        assertEquals(1, PerfStartup.startupType["cold_main"].testGetValue())
    }

    @Test
    fun `GIVEN the activity is launched WHEN onResume is called THEN we record the telemetry`() {
        launchApp()
        verify(exactly = 1) { telemetry.record() }
    }

    @Test
    fun `GIVEN the activity is launched WHEN the activity is paused and resumed THEN record is not called`() {
        // This part of the test duplicates another test but it's needed to initialize the state of this test.
        launchApp()
        verify(exactly = 1) { telemetry.record() }

        callbacks.onPause(mockk())
        callbacks.onResume(mockk())

        verify(exactly = 1) { telemetry.record() } // i.e. this shouldn't be called again.
    }

    @Test
    fun `GIVEN the activity is launched WHEN the activity is stopped and resumed THEN record is called again`() {
        // This part of the test duplicates another test but it's needed to initialize the state of this test.
        launchApp()
        verify(exactly = 1) { telemetry.record() }

        callbacks.onPause(mockk())
        callbacks.onStop(mockk())
        callbacks.onStart(mockk())
        callbacks.onResume(mockk())

        verify(exactly = 2) { telemetry.record() } // i.e. this should be called again.
    }

    private fun launchApp() {
        // What these return isn't important.
        every { stateProvider.getStartupStateForStartedActivity(activityClass) } returns StartupState.COLD
        every { pathProvider.startupPathForActivity } returns StartupPath.MAIN

        callbacks.onCreate(mockk())
        callbacks.onStart(mockk())
        callbacks.onResume(mockk())
    }
}

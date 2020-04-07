/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import io.mockk.Called
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.service.glean.private.BooleanMetricType
import mozilla.components.service.glean.private.CounterMetricType
import mozilla.components.service.glean.private.TimespanMetricType
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.GleanMetrics.StartupTimeline as Telemetry

private const val CLOCK_TICKS_PER_SECOND = 100L

class StartupFrameworkStartMeasurementTest {

    private lateinit var metrics: StartupFrameworkStartMeasurement
    private lateinit var stat: Stat

    // We'd prefer to use the Glean test methods over these mocks but they require us to add
    // Robolectric and it's not worth the impact on test duration.
    @MockK private lateinit var telemetry: Telemetry
    @MockK(relaxed = true) private lateinit var frameworkStart: TimespanMetricType
    @MockK(relaxed = true) private lateinit var frameworkStartError: BooleanMetricType
    @MockK(relaxed = true) private lateinit var clockTicksPerSecond: CounterMetricType

    private var elapsedRealtimeNanos = -1L
    private var processStartTimeTicks = -1L

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        elapsedRealtimeNanos = -1
        processStartTimeTicks = -1

        stat = spyk(object : Stat() {
            override val clockTicksPerSecond: Long get() = CLOCK_TICKS_PER_SECOND
        })
        every { stat.getProcessStartTimeTicks(any()) } answers { processStartTimeTicks }
        val getElapsedRealtimeNanos = { elapsedRealtimeNanos }

        every { telemetry.frameworkStart } returns frameworkStart
        every { telemetry.frameworkStartError } returns frameworkStartError
        every { telemetry.clockTicksPerSecond } returns clockTicksPerSecond

        metrics = StartupFrameworkStartMeasurement(stat, telemetry, getElapsedRealtimeNanos)
    }

    @Test
    fun `GIVEN app init is invalid WHEN metrics are set THEN frameworkStartError is set to true`() {
        setProcessAppInitAndMetrics(processStart = 10, appInit = -1)
        verifyFrameworkStartError()
    }

    @Test
    fun `GIVEN app init is not called WHEN metrics are set THEN frameworkStartError is set to true`() {
        metrics.setExpensiveMetric()
        verifyFrameworkStartError()
    }

    @Test
    fun `GIVEN app init is set to valid values WHEN metrics are set THEN frameworkStart is set with the correct value`() {
        setProcessAppInitAndMetrics(processStart = 166_636_813, appInit = 1_845_312_345_673_925)
        verifyFrameworkStartSuccess(178_944_220_000_000) // calculated by hand.
    }

    @Test // this overlaps with the success case test.
    fun `GIVEN app init has valid values WHEN onAppInit is called twice and metrics are set THEN frameworkStart uses the first app init value`() {
        processStartTimeTicks = 166_636_813

        elapsedRealtimeNanos = 1_845_312_345_673_925
        metrics.onApplicationInit()
        elapsedRealtimeNanos = 1_945_312_345_673_925
        metrics.onApplicationInit()

        metrics.setExpensiveMetric()
        verifyFrameworkStartSuccess(178_944_220_000_000) // calculated by hand.
    }

    @Test
    fun `GIVEN app init have valid values WHEN metrics are set twice THEN frameworkStart is only set once`() {
        setProcessAppInitAndMetrics(10, 100)
        metrics.setExpensiveMetric()
        verify(exactly = 1) { frameworkStart.setRawNanos(any()) }
        verify(exactly = 1) { clockTicksPerSecond.add(any()) }
        verify { frameworkStartError wasNot Called }
    }

    private fun setProcessAppInitAndMetrics(processStart: Long, appInit: Long) {
        processStartTimeTicks = processStart

        elapsedRealtimeNanos = appInit
        metrics.onApplicationInit()

        metrics.setExpensiveMetric()
    }

    private fun verifyFrameworkStartSuccess(nanos: Long) {
        verify { frameworkStart.setRawNanos(nanos) }
        verify { clockTicksPerSecond.add(CLOCK_TICKS_PER_SECOND.toInt()) }
        verify { frameworkStartError wasNot Called }
    }

    private fun verifyFrameworkStartError() {
        verify { frameworkStartError.set(true) }
        verify { frameworkStart wasNot Called }
        verify { clockTicksPerSecond wasNot Called }
    }
}

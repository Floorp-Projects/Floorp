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

private const val PRIMARY_TICKS = 100L
private const val SECONDARY_TICKS = 50L

class StartupFrameworkStartMeasurementTest {

    private lateinit var metrics: StartupFrameworkStartMeasurement
    private lateinit var stat: Stat

    // We'd prefer to use the Glean test methods over these mocks but they require us to add
    // Robolectric and it's not worth the impact on test duration.
    @MockK private lateinit var telemetry: Telemetry
    @MockK(relaxed = true) private lateinit var frameworkPrimary: TimespanMetricType
    @MockK(relaxed = true) private lateinit var frameworkSecondary: TimespanMetricType
    @MockK(relaxed = true) private lateinit var frameworkStartError: BooleanMetricType
    @MockK(relaxed = true) private lateinit var clockTicksPerSecond: CounterMetricType

    private var clockTicksPerSecondValue = -1L

    private var elapsedRealtimeNanos = -1L
    private var processStartTimeTicks = -1L

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        elapsedRealtimeNanos = -1
        processStartTimeTicks = -1

        // This value is hard-coded in the OS so we default to it as it's the expected value.
        clockTicksPerSecondValue = PRIMARY_TICKS

        stat = spyk(object : Stat() {
            override val clockTicksPerSecond: Long get() = clockTicksPerSecondValue
        })
        every { stat.getProcessStartTimeTicks(any()) } answers { processStartTimeTicks }
        val getElapsedRealtimeNanos = { elapsedRealtimeNanos }

        every { telemetry.frameworkPrimary } returns frameworkPrimary
        every { telemetry.frameworkSecondary } returns frameworkSecondary
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
    fun `GIVEN app init is set to valid values and clock ticks per second is 100 WHEN metrics are set THEN frameworkPrimary is set with the correct value`() {
        clockTicksPerSecondValue = PRIMARY_TICKS
        setProcessAppInitAndMetrics(processStart = 166_636_813, appInit = 1_845_312_345_673_925)
        verifyFrameworkStartSuccess(178_944_220_000_000, isPrimary = true) // calculated by hand.
    }

    @Test
    fun `GIVEN app init is set to valid values and clock ticks per second is not 100 WHEN metrics are set THEN frameworkSecondary is set with the correct value`() {
        clockTicksPerSecondValue = SECONDARY_TICKS
        setProcessAppInitAndMetrics(processStart = 166_636_813, appInit = 1_845_312_345_673_925)
        verifyFrameworkStartSuccess(178_944_220_000_000, isPrimary = false) // calculated by hand.
    }

    @Test // this overlaps with the success case test.
    fun `GIVEN app init has valid values and clock ticks per second is 100 WHEN onAppInit is called twice and metrics are set THEN frameworkPrimary uses the first app init value`() {
        clockTicksPerSecondValue = PRIMARY_TICKS
        testAppInitCalledTwice(isPrimary = true)
    }

    @Test // this overlaps with the success case test.
    fun `GIVEN app init has valid values and clock ticks per second is not 100 WHEN onAppInit is called twice and metrics are set THEN frameworkSecondary uses the first app init value`() {
        clockTicksPerSecondValue = SECONDARY_TICKS
        testAppInitCalledTwice(isPrimary = false)
    }

    private fun testAppInitCalledTwice(isPrimary: Boolean) {
        processStartTimeTicks = 166_636_813

        elapsedRealtimeNanos = 1_845_312_345_673_925
        metrics.onApplicationInit()
        elapsedRealtimeNanos = 1_945_312_345_673_925
        metrics.onApplicationInit()

        metrics.setExpensiveMetric()
        verifyFrameworkStartSuccess(178_944_220_000_000, isPrimary) // calculated by hand.
    }

    @Test
    fun `GIVEN app init have valid values and clock ticks per second is 100 WHEN metrics are set twice THEN frameworkPrimary is only set once`() {
        clockTicksPerSecondValue = PRIMARY_TICKS
        testMetricsSetTwice(isPrimary = true)
    }

    @Test
    fun `GIVEN app init have valid values and clock ticks per second is not 100 WHEN metrics are set twice THEN frameworkSecondary is only set once`() {
        clockTicksPerSecondValue = SECONDARY_TICKS
        testMetricsSetTwice(isPrimary = false)
    }

    private fun testMetricsSetTwice(isPrimary: Boolean) {
        setProcessAppInitAndMetrics(10, 100)
        metrics.setExpensiveMetric()

        val (setMetric, unsetMetric) = getSetAndUnsetMetric(isPrimary)
        verify(exactly = 1) { setMetric.setRawNanos(any()) }
        verify { unsetMetric wasNot Called }
        verify(exactly = 1) { clockTicksPerSecond.add(any()) }
        verify { frameworkStartError wasNot Called }
    }

    private fun setProcessAppInitAndMetrics(processStart: Long, appInit: Long) {
        processStartTimeTicks = processStart

        elapsedRealtimeNanos = appInit
        metrics.onApplicationInit()

        metrics.setExpensiveMetric()
    }

    private fun verifyFrameworkStartSuccess(nanos: Long, isPrimary: Boolean) {
        val (setMetric, unsetMetric) = getSetAndUnsetMetric(isPrimary)
        verify { setMetric.setRawNanos(nanos) }
        verify { unsetMetric wasNot Called }

        val expectedClockTicksPerSecond = getExpectedClockTicksPerSecond(isPrimary)
        verify { clockTicksPerSecond.add(expectedClockTicksPerSecond.toInt()) }
        verify { frameworkStartError wasNot Called }
    }

    private fun verifyFrameworkStartError() {
        verify { frameworkStartError.set(true) }
        verify { frameworkPrimary wasNot Called }
        verify { frameworkSecondary wasNot Called }
        verify { clockTicksPerSecond wasNot Called }
    }

    private fun getSetAndUnsetMetric(isPrimary: Boolean): Pair<TimespanMetricType, TimespanMetricType> {
        return if (isPrimary) {
            Pair(frameworkPrimary, frameworkSecondary)
        } else {
            Pair(frameworkSecondary, frameworkPrimary)
        }
    }

    // This hard-codes some data that's passed into the test but I don't want to spend more time
    // so I don't bother cleaning it up now.
    private fun getExpectedClockTicksPerSecond(isPrimary: Boolean): Long =
        if (isPrimary) PRIMARY_TICKS else SECONDARY_TICKS
}

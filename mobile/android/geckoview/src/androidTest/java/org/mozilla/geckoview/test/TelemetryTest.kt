/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.CoreMatchers.equalTo
import org.hamcrest.Matchers.* // ktlint-disable no-wildcard-imports
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.RuntimeTelemetry
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled

@RunWith(AndroidJUnit4::class)
@MediumTest
class TelemetryTest : BaseSessionTest() {
    @Test
    fun testOnTelemetryReceived() {
        // Let's make sure we batch the telemetry calls.
        sessionRule.setPrefsUntilTestEnd(
            mapOf("toolkit.telemetry.geckoview.batchDurationMS" to 100000),
        )

        val expectedHistograms = listOf<Long>(401, 12, 1, 109, 2000)
        val receivedHistograms = mutableListOf<Long>()
        val histogram = GeckoResult<Void>()
        val stringScalar = GeckoResult<Void>()
        val booleanScalar = GeckoResult<Void>()
        val longScalar = GeckoResult<Void>()

        sessionRule.addExternalDelegateUntilTestEnd(
            RuntimeTelemetry.Delegate::class,
            sessionRule::setTelemetryDelegate,
            { sessionRule.setTelemetryDelegate(null) },
            object : RuntimeTelemetry.Delegate {
                @AssertCalled
                override fun onHistogram(metric: RuntimeTelemetry.Histogram) {
                    if (metric.name != "TELEMETRY_TEST_STREAMING") {
                        return
                    }

                    assertThat(
                        "The histogram should not be categorical",
                        metric.isCategorical,
                        equalTo(false),
                    )

                    receivedHistograms.addAll(metric.value.toList())

                    if (receivedHistograms.size == expectedHistograms.size) {
                        histogram.complete(null)
                    }
                }

                @AssertCalled
                override fun onStringScalar(metric: RuntimeTelemetry.Metric<String>) {
                    if (metric.name != "telemetry.test.string_kind") {
                        return
                    }

                    assertThat(
                        "Metric value should match",
                        metric.value,
                        equalTo("test scalar"),
                    )

                    stringScalar.complete(null)
                }

                @AssertCalled
                override fun onBooleanScalar(metric: RuntimeTelemetry.Metric<Boolean>) {
                    if (metric.name != "telemetry.test.boolean_kind") {
                        return
                    }

                    assertThat(
                        "Metric value should match",
                        metric.value,
                        equalTo(true),
                    )

                    booleanScalar.complete(null)
                }

                @AssertCalled
                override fun onLongScalar(metric: RuntimeTelemetry.Metric<Long>) {
                    if (metric.name != "telemetry.test.unsigned_int_kind") {
                        return
                    }

                    assertThat(
                        "Metric value should match",
                        metric.value,
                        equalTo(1234L),
                    )

                    longScalar.complete(null)
                }
            },
        )

        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", expectedHistograms[0])
        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", expectedHistograms[1])
        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", expectedHistograms[2])
        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", expectedHistograms[3])

        sessionRule.setScalar("telemetry.test.boolean_kind", true)
        sessionRule.setScalar("telemetry.test.unsigned_int_kind", 1234)
        sessionRule.setScalar("telemetry.test.string_kind", "test scalar")

        // Forces flushing telemetry data at next histogram.
        sessionRule.setPrefsUntilTestEnd(
            mapOf("toolkit.telemetry.geckoview.batchDurationMS" to 0),
        )
        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", expectedHistograms[4])

        sessionRule.waitForResult(histogram)
        sessionRule.waitForResult(stringScalar)
        sessionRule.waitForResult(booleanScalar)
        sessionRule.waitForResult(longScalar)

        assertThat(
            "Metric values should match",
            receivedHistograms,
            equalTo(expectedHistograms),
        )
    }
}

/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import org.hamcrest.CoreMatchers.equalTo
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
        // Let's make sure we batch the two telemetry calls
        sessionRule.setPrefsUntilTestEnd(
                mapOf("toolkit.telemetry.geckoview.batchDurationMS" to 100000))

        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", 401)
        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", 12)
        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", 1)
        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", 109)

        sessionRule.setScalar("telemetry.test.boolean_kind", true)
        sessionRule.setScalar("telemetry.test.unsigned_int_kind", 1234)
        sessionRule.setScalar("telemetry.test.string_kind", "test scalar")

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

                assertThat("Metric name should be correct", metric.value,
                        equalTo(longArrayOf(401, 12, 1, 109, 2000)))
                assertThat("The histogram should not be categorical", metric.isCategorical,
                        equalTo(false))
                histogram.complete(null)
            }

            @AssertCalled
            override fun onStringScalar(metric: RuntimeTelemetry.Metric<String>) {
                if (metric.name != "telemetry.test.string_kind") {
                    return
                }

                assertThat("Metric name should be correct", metric.value,
                        equalTo("test scalar"))
                stringScalar.complete(null)
            }

            @AssertCalled
            override fun onBooleanScalar(metric: RuntimeTelemetry.Metric<Boolean>) {
                if (metric.name != "telemetry.test.boolean_kind") {
                    return
                }

                assertThat("Metric name should be correct", metric.value,
                        equalTo(true))
                booleanScalar.complete(null)
            }

            @AssertCalled
            override fun onLongScalar(metric: RuntimeTelemetry.Metric<Long>) {
                if (metric.name != "telemetry.test.unsigned_int_kind") {
                    return
                }

                assertThat("Metric name should be correct", metric.value,
                        equalTo(1234L))
                longScalar.complete(null)
            }
        })

        // Forces flushing telemetry data at next histogram
        sessionRule.setPrefsUntilTestEnd(mapOf("toolkit.telemetry.geckoview.batchDurationMS" to 0))
        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", 2000)

        sessionRule.waitForResult(histogram)
        sessionRule.waitForResult(stringScalar)
        sessionRule.waitForResult(booleanScalar)
        sessionRule.waitForResult(longScalar)
    }
}

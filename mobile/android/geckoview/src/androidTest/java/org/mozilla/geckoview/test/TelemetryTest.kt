/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import android.support.test.filters.MediumTest
import android.support.test.runner.AndroidJUnit4
import android.util.Log
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
        sessionRule.addExternalDelegateUntilTestEnd(
            RuntimeTelemetry.Delegate::class,
                sessionRule::setTelemetryDelegate,
                { sessionRule.setTelemetryDelegate(null) },
                object : RuntimeTelemetry.Delegate {}
        )

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

        // Forces flushing telemetry data at next histogram
        sessionRule.setPrefsUntilTestEnd(mapOf("toolkit.telemetry.geckoview.batchDurationMS" to 0))
        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", 2000)

        sessionRule.waitUntilCalled(object : RuntimeTelemetry.Delegate {
            @AssertCalled
            override fun onHistogram(metric: RuntimeTelemetry.Histogram) {
                assertThat("Metric name should be correct", metric.name,
                        equalTo("TELEMETRY_TEST_STREAMING"))
                assertThat("Metric name should be correct", metric.value,
                        equalTo(longArrayOf(401, 12, 1, 109, 2000)))
                assertThat("The histogram should not be categorical", metric.isCategorical,
                        equalTo(false))
            }

            @AssertCalled
            override fun onStringScalar(metric: RuntimeTelemetry.Metric<String>) {
                assertThat("Metric name should be correct", metric.name,
                        equalTo("telemetry.test.string_kind"))
                assertThat("Metric name should be correct", metric.value,
                        equalTo("test scalar"))
            }

            @AssertCalled
            override fun onBooleanScalar(metric: RuntimeTelemetry.Metric<Boolean>) {
                assertThat("Metric name should be correct", metric.name,
                        equalTo("telemetry.test.boolean_kind"))
                assertThat("Metric name should be correct", metric.value,
                        equalTo(true))
            }

            @AssertCalled
            override fun onLongScalar(metric: RuntimeTelemetry.Metric<Long>) {
                assertThat("Metric name should be correct", metric.name,
                        equalTo("telemetry.test.unsigned_int_kind"))
                assertThat("Metric name should be correct", metric.value,
                        equalTo(1234L))
            }
        })
    }
}

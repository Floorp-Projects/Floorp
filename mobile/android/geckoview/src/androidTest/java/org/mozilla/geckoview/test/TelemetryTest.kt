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

        // Forces flushing telemetry data at next histogram
        sessionRule.setPrefsUntilTestEnd(mapOf("toolkit.telemetry.geckoview.batchDurationMS" to 0))

        val telemetryReceived = GeckoResult<Void>()
        sessionRule.delegateDuringNextWait(object : RuntimeTelemetry.Delegate {
            @AssertCalled
            override fun onTelemetryReceived(metric: RuntimeTelemetry.Metric) {
                assertThat("Metric name should be correct", metric.name,
                        equalTo("TELEMETRY_TEST_STREAMING"))
                assertThat("Metric name should be correct", metric.values,
                        equalTo(longArrayOf(401, 12, 1, 109, 2000)))
                telemetryReceived.complete(null)
            }
        })

        sessionRule.addHistogram("TELEMETRY_TEST_STREAMING", 2000)
        sessionRule.waitForResult(telemetryReceived)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.MockK
import io.mockk.verifySequence
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.service.glean.private.NoReasonCodes
import mozilla.components.service.glean.private.PingType
import org.junit.Before
import org.junit.Test

@ExperimentalCoroutinesApi
class StartupHomeActivityLifecycleObserverTest {

    @MockK(relaxed = true) private lateinit var frameworkStartMeasurement: StartupFrameworkStartMeasurement
    @MockK(relaxed = true) private lateinit var startupTimeline: PingType<NoReasonCodes>

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
    }

    @Test
    fun `WHEN onStop is called THEN the metrics are set and the ping is submitted`() = runBlockingTest {
        val observer = StartupHomeActivityLifecycleObserver(frameworkStartMeasurement, startupTimeline, this)
        observer.onStop()

        verifySequence {
            frameworkStartMeasurement.setExpensiveMetric()
            startupTimeline.submit()
        }
    }
}

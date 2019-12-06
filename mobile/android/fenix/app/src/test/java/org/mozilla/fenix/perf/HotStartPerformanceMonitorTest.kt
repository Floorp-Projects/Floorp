/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import io.mockk.Called
import io.mockk.MockKAnnotations
import io.mockk.impl.annotations.MockK
import io.mockk.verify
import org.junit.Before
import org.junit.Test

class HotStartPerformanceMonitorTest {

    private lateinit var monitor: HotStartPerformanceMonitor

    @MockK(relaxed = true) private lateinit var log: (String) -> Unit
    private var elapsedRealtime = 0L

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
        monitor = HotStartPerformanceMonitor(log, getElapsedRealtime = { elapsedRealtime })
    }

    @Test
    fun `WHEN onRestart is not called but onPostResume is called THEN we do not log`() {
        monitor.onPostResumeFinalMethodCall()
        verify { log.invoke(any()) wasNot Called }
    }

    @Test
    fun `WHEN onRestart then onPostResume is called THEN we log the elapsed time`() {
        elapsedRealtime = 10
        monitor.onRestartFirstMethodCall()

        elapsedRealtime = 100
        monitor.onPostResumeFinalMethodCall()

        verify { log.invoke("hot start: 90") } // fragile but it's not worth the time to make robust.
    }
}

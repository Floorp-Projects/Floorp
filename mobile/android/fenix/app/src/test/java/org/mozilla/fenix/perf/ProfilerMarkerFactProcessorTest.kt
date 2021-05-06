/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.os.Handler
import android.os.Looper
import io.mockk.Called
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import org.junit.Before
import org.junit.Test

class ProfilerMarkerFactProcessorTest {

    @RelaxedMockK lateinit var profiler: Profiler
    @RelaxedMockK lateinit var mainHandler: Handler
    lateinit var processor: ProfilerMarkerFactProcessor

    var myLooper: Looper? = null

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
        myLooper = null
        processor = ProfilerMarkerFactProcessor({ profiler }, mainHandler, { myLooper })
    }

    @Test
    fun `GIVEN we are on the main thread WHEN a fact with an implementation detail action is received THEN a profiler marker is added now`() {
        myLooper = mainHandler.looper // main thread

        val fact = newFact(Action.IMPLEMENTATION_DETAIL)
        processor.process(fact)

        verify { profiler.addMarker(fact.item) }
    }

    @Test
    fun `GIVEN we are not on the main thread WHEN a fact with an implementation detail action is received THEN adding the marker is posted to the main thread`() {
        myLooper = mockk() // off main thread
        val mainThreadPostedSlot = slot<Runnable>()
        every { profiler.getProfilerTime() } returns 100.0

        val fact = newFact(Action.IMPLEMENTATION_DETAIL)
        processor.process(fact)

        verify { mainHandler.post(capture(mainThreadPostedSlot)) }
        verifyProfilerAddMarkerWasNotCalled()

        mainThreadPostedSlot.captured.run() // call the captured function posted to the main thread.
        verify { profiler.addMarker(fact.item, 100.0, 100.0, null) }
    }

    @Test
    fun `WHEN a fact with a non-implementation detail action is received THEN no profiler marker is added`() {
        val fact = newFact(Action.CANCEL)
        processor.process(fact)
        verify { profiler wasNot Called }
    }

    private fun verifyProfilerAddMarkerWasNotCalled() {
        verify(exactly = 0) {
            profiler.addMarker(any())
            profiler.addMarker(any(), any() as Double?)
            profiler.addMarker(any(), any() as String?)
            profiler.addMarker(any(), any(), any())
            profiler.addMarker(any(), any(), any(), any())
        }
    }
}

private fun newFact(
    action: Action,
    item: String = "itemName"
) = Fact(
    Component.BROWSER_STATE,
    action,
    item
)

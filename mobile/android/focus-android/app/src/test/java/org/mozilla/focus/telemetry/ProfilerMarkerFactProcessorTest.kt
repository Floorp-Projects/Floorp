/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.os.Handler
import android.os.Looper
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions
import org.mozilla.focus.any
import org.mozilla.focus.argumentCaptor
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ProfilerMarkerFactProcessorTest {

    private val profiler: Profiler = mock(Profiler::class.java)
    private val mainHandler: Handler = mock(Handler::class.java)
    lateinit var processor: ProfilerMarkerFactProcessor

    var myLooper: Looper? = null

    @Before
    fun setUp() {
        myLooper = null
        processor = ProfilerMarkerFactProcessor({ profiler }, mainHandler, { myLooper })
    }

    @Test
    fun `GIVEN we are on the main thread WHEN a fact with an implementation detail action is received THEN a profiler marker is added now`() {
        myLooper = mainHandler.looper // main thread

        val fact = newFact(Action.IMPLEMENTATION_DETAIL)
        processor.process(fact)

        verify(profiler).addMarker(fact.item)
    }

    @Test
    fun `GIVEN we are not on the main thread WHEN a fact with an implementation detail action is received THEN adding the marker is posted to the main thread`() {
        myLooper = mock(Looper::class.java) // off main thread
        val mainThreadPostedArg = argumentCaptor<Runnable>()
        `when`(profiler.getProfilerTime()).thenReturn(100.0)

        val fact = newFact(Action.IMPLEMENTATION_DETAIL)
        processor.process(fact)

        verify(mainHandler).post(mainThreadPostedArg.capture())
        verifyProfilerAddMarkerWasNotCalled()

        mainThreadPostedArg.value.run()
        verify(profiler).addMarker(fact.item, 100.0, 100.0, null)
    }

    @Test
    fun `WHEN a fact with a non-implementation detail action is received THEN no profiler marker is added`() {
        val fact = newFact(Action.CANCEL)
        processor.process(fact)
        verifyZeroInteractions(profiler)
    }

    private fun verifyProfilerAddMarkerWasNotCalled() {
        verify(profiler, never()).addMarker(any())
        verify(profiler, never()).addMarker(any(), any() as Double?)
        verify(profiler, never()).addMarker(any(), any() as String?)
        verify(profiler, never()).addMarker(any(), any(), any())
        verify(profiler, never()).addMarker(any(), any(), any(), any())
    }
}

private fun newFact(
    action: Action,
    item: String = "itemName"
) = Fact(
    Component.BROWSER_SESSION_STORAGE,
    action,
    item
)
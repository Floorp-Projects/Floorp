/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import android.os.Handler
import android.os.Looper
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor

/**
 * A fact processor that adds Gecko profiler markers for [Fact]s matching a specific format.
 * We look for the following format:
 * ```
 * Fact(
 *     action = Action.IMPLEMENTATION_DETAIL
 *     item = <marker name>
 * )
 * ```
 *
 * This allows us to add profiler markers from android-components code. Using the Fact API for this
 * purpose, rather than calling [Profiler.addMarker] directly inside components, has trade-offs. Its
 * downsides are that it is less explicit and tooling does not work as well on it. However, we felt
 * it was worthwhile because:
 *
 * 1. we don't know what profiler markers are useful so we want to be able to iterate quickly.
 * Adding dependencies on the Profiler and landing these changes across two repos hinders that
 * 2. we want to instrument the code as close to specific method calls as possible (e.g.
 * GeckoSession.loadUrl) but it's not always easy to do so (e.g. in the previous example, passing a
 * Profiler reference to GeckoEngineSession is difficult because GES is not a global dependency)
 * 3. we can only add Profiler markers from the main thread so adding markers will become more
 * difficult if we have to understand the threading needs of each Profiler call site
 *
 * An additional benefit with having this infrastructure is that it's easy to add Profiler markers
 * for local debugging.
 *
 * That being said, if we find a location where it would be valuable to have a long term Profiler
 * marker, we should consider instrumenting it via the [Profiler] API.
 */
class ProfilerMarkerFactProcessor @VisibleForTesting(otherwise = PRIVATE) constructor(
    // We use a provider to defer accessing the profiler until we need it, because the property is a
    // child of the engine property and we don't want to initialize it earlier than we intend to.
    private val profilerProvider: () -> Profiler?,
    private val mainHandler: Handler = Handler(Looper.getMainLooper()),
    private val getMyLooper: () -> Looper? = { Looper.myLooper() }
) : FactProcessor {

    override fun process(fact: Fact) {
        if (fact.action != Action.IMPLEMENTATION_DETAIL) {
            return
        }

        val markerName = fact.item

        // Java profiler markers can only be added from the main thread so, for now, we push all
        // markers to the the main thread (which also groups all the markers together,
        // making it easier to read).
        val profiler = profilerProvider()
        if (getMyLooper() == mainHandler.looper) {
            profiler?.addMarker(markerName)
        } else {
            // To reduce the performance burden, we could early return if the profiler isn't active.
            // However, this would change the performance characteristics from when the profiler is
            // active and when it's inactive so we always post instead.
            val now = profiler?.getProfilerTime()
            mainHandler.post {
                // We set now to both start and end time because we want a marker of without duration
                // and if end is omitted, the duration is created implicitly.
                profiler?.addMarker(markerName, now, now, null)
            }
        }
    }

    companion object {
        fun create(profilerProvider: () -> Profiler?) =
            ProfilerMarkerFactProcessor(profilerProvider)
    }
}
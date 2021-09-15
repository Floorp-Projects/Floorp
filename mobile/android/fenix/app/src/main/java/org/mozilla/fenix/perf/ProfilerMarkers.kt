/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.Activity
import android.view.View
import android.view.ViewTreeObserver
import androidx.core.view.doOnPreDraw
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.engine.Engine

/**
 * A container for functions for when adding a profiler marker is less readable
 * (e.g. multiple lines, more advanced logic).
 */
object ProfilerMarkers {

    fun addListenerForOnGlobalLayout(engine: Engine, activity: Activity, rootView: View) {
        // We define the listener in a non-anonymous class to avoid memory leaks with the activity.
        val listener = MarkerGlobalLayoutListener(engine, activity::class.simpleName ?: "null")
        rootView.viewTreeObserver.addOnGlobalLayoutListener(listener)
    }

    fun homeActivityOnStart(rootContainer: View, profiler: Profiler?) {
        rootContainer.doOnPreDraw {
            profiler?.addMarker("onPreDraw", "expected first frame via HomeActivity.onStart")
        }
    }
}

/**
 * A global layout listener that adds a profiler marker on global layout.
 */
class MarkerGlobalLayoutListener(
    private val engine: Engine,
    private val activityName: String,
) : ViewTreeObserver.OnGlobalLayoutListener {
    override fun onGlobalLayout() {
        engine.profiler?.addMarker("onGlobalLayout", activityName)
    }
}

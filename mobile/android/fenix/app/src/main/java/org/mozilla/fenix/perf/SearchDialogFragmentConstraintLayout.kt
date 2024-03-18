/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.content.Context
import android.graphics.Canvas
import android.util.AttributeSet
import androidx.constraintlayout.widget.ConstraintLayout
import mozilla.components.concept.base.profiler.Profiler
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.perf.ProfilerMarkers.MEASURE_LAYOUT_DRAW_MARKER_NAME
import org.mozilla.fenix.search.SearchDialogFragment

/**
 * Adds markers for measure/layout/draw to the root layout of [SearchDialogFragment].
 */
class SearchDialogFragmentConstraintLayout(context: Context, attrs: AttributeSet) : ConstraintLayout(context, attrs) {

    private val profiler: Profiler? = context.components.core.engine.profiler

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val profilerStartTime = profiler?.getProfilerTime()
        super.onMeasure(widthMeasureSpec, heightMeasureSpec)
        profiler?.addMarker(MEASURE_LAYOUT_DRAW_MARKER_NAME, profilerStartTime, "onMeasure (SearchDialogFragment root)")
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        val profilerStartTime = profiler?.getProfilerTime()
        super.onLayout(changed, left, top, right, bottom)
        profiler?.addMarker(MEASURE_LAYOUT_DRAW_MARKER_NAME, profilerStartTime, "onLayout (SearchDialogFragment root)")
    }

    override fun draw(canvas: Canvas) {
        // We instrument draw, rather than onDraw or dispatchDraw, because ConstraintLayout's draw includes
        // both of the other methods. If we want to track how long it takes to draw the children,
        // we'd get more information by instrumenting them individually.
        val profilerStartTime = profiler?.getProfilerTime()
        super.draw(canvas)
        profiler?.addMarker(MEASURE_LAYOUT_DRAW_MARKER_NAME, profilerStartTime, "draw (SearchDialogFragment root)")
    }
}

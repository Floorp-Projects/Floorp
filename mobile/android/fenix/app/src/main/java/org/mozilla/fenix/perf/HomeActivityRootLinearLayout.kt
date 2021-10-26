/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.content.Context
import android.util.AttributeSet
import android.widget.LinearLayout
import mozilla.components.concept.base.profiler.Profiler
import org.mozilla.fenix.HomeActivity

private const val DETAIL_TEXT = "RootLinearLayout"

/**
 * A [LinearLayout] that adds profiler markers for various methods. This is intended to be used on
 * the root view of [HomeActivity]'s view hierarchy to understand global measure/layout events.
 */
class HomeActivityRootLinearLayout(context: Context, attrs: AttributeSet) : LinearLayout(context, attrs) {

    var profilerProvider: () -> Profiler? = { null }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val profilerStartTime = profilerProvider.invoke()?.getProfilerTime()
        super.onMeasure(widthMeasureSpec, heightMeasureSpec)
        profilerProvider.invoke()?.addMarker("onMeasure", profilerStartTime, DETAIL_TEXT)
    }

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
        val profilerStartTime = profilerProvider.invoke()?.getProfilerTime()
        super.onLayout(changed, l, t, r, b)
        profilerProvider.invoke()?.addMarker("onLayout", profilerStartTime, DETAIL_TEXT)
    }
}

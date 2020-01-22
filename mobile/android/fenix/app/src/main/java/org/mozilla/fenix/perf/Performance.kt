/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import androidx.core.view.doOnPreDraw
import kotlinx.android.synthetic.main.activity_home.*
import org.mozilla.fenix.HomeActivity

/**
 * A collection of objects related to app performance.
 */
object Performance {
    const val TAG = "FenixPerf"

    /**
     * Instruments cold startup time for use with our internal measuring system, FNPRMS. This may
     * also appear in Google Play Vitals dashboards.
     *
     * This will need to be rewritten if any parts of the UI are changed to be displayed
     * asynchronously.
     *
     * In the current implementation, we only intend to instrument cold startup to the homescreen.
     * To save implementation time, we ignore the fact that the RecyclerView draws twice if the user
     * has tabs, collections, etc. open: the "No tabs" placeholder and a tab list. This
     * instrumentation will only capture the "No tabs" draw.
     */
    fun instrumentColdStartupToHomescreenTime(activity: HomeActivity) {
        // For greater accuracy, we could add an onDrawListener instead of a preDrawListener but:
        // - single use onDrawListeners are not built-in and it's non-trivial to write one
        // - the difference in timing is minimal (< 7ms on Pixel 2)
        // - if we compare against another app using a preDrawListener, it should be comparable
        //
        // Unfortunately, this is tightly coupled to the root view of HomeActivity's view hierarchy
        activity.rootContainer.doOnPreDraw {
            activity.reportFullyDrawn()
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.Activity
import android.view.View
import androidx.core.view.doOnPreDraw
import mozilla.components.support.ktx.android.view.reportFullyDrawnSafe
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.home.topsites.TopSiteItemViewHolder
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupDestination.APP_LINK
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupDestination.HOMESCREEN
import org.mozilla.fenix.perf.StartupTimelineStateMachine.StartupState

/**
 * Instruments the Android framework method [Activity.reportFullyDrawn], which prints time to visual
 * completeness to logcat.
 *
 * At the time of writing (2020-02-26), this functionality is tightly coupled to FNPRMS, our internal
 * startup measurement system. However, these values may also appear in the Google Play Vitals
 * dashboards.
 */
class StartupReportFullyDrawn {

    // Ideally we'd incorporate this state into the StartupState but we're short on implementation time.
    private var isInstrumented = false

    /**
     * Instruments "visually complete" cold startup time for app link for use with FNPRMS.
     */
    fun onActivityCreateEndHome(state: StartupState, activity: HomeActivity) {
        if (!isInstrumented &&
            state is StartupState.Cold && state.destination == APP_LINK
        ) {
            // Instrumenting the first frame drawn should be good enough for app link for now.
            isInstrumented = true
            attachReportFullyDrawn(activity, activity.findViewById(R.id.rootContainer))
        }
    }

    /**
     * Instruments "visually complete" cold startup time to homescreen for use with FNPRMS.
     *
     * For FNPRMS, we define "visually complete" to be when top sites is loaded with placeholders;
     * the animation to display top sites will occur after this point, as will the asynchronous
     * loading of the actual top sites icons. Our focus for visually complete is usability.
     * There are no tabs available in our FNPRMS tests so they are ignored for this instrumentation.
     */
    fun onTopSitesItemBound(state: StartupState, holder: TopSiteItemViewHolder) {
        if (!isInstrumented &&
            state is StartupState.Cold && state.destination == HOMESCREEN
        ) {
            isInstrumented = true

            // Ideally we wouldn't cast to HomeActivity but we want to save implementation time.
            val view = holder.itemView
            attachReportFullyDrawn(view.context as HomeActivity, view)
        }
    }

    /**
     * Instruments "visually complete" cold startup time to homescreen for use with FNPRMS.
     *
     * For FNPRMS, we define "visually complete" to be when top sites is loaded with placeholders;
     * the animation to display top sites will occur after this point, as will the asynchronous
     * loading of the actual top sites icons. Our focus for visually complete is usability.
     * There are no tabs available in our FNPRMS tests so they are ignored for this instrumentation.
     */
    fun onTopSitesItemBound(state: StartupState, activity: HomeActivity) {
        if (!isInstrumented &&
            state is StartupState.Cold && state.destination == HOMESCREEN
        ) {
            isInstrumented = true

            attachReportFullyDrawn(activity)
        }
    }

    private fun attachReportFullyDrawn(activity: Activity, view: View) {
        // For greater accuracy, we could add an onDrawListener instead of a preDrawListener but:
        // - single use onDrawListeners are not built-in and it's non-trivial to write one
        // - the difference in timing is minimal (< 7ms on Pixel 2)
        // - if we compare against another app using a preDrawListener, as we are with Fennec, it
        // should be comparable
        view.doOnPreDraw { activity.reportFullyDrawnSafe(Performance.logger) }
    }

    private fun attachReportFullyDrawn(activity: Activity) {
        activity.reportFullyDrawnSafe(Performance.logger)
    }
}

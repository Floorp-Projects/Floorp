/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import androidx.core.view.doOnPreDraw
import kotlinx.android.synthetic.main.activity_home.*
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.onboarding.FenixOnboarding
import android.provider.Settings as AndroidSettings
import org.mozilla.fenix.utils.Settings

/**
 * A collection of objects related to app performance.
 */
object Performance {
    const val TAG = "FenixPerf"
    private const val EXTRA_IS_PERFORMANCE_TEST = "performancetest"

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

    /**
     * Processes intent for Performance testing to remove protection pop up ( but keeps the TP
     * on) and removes the onboarding screen.
     */
    fun processIntentIfPerformanceTest(intent: Intent, context: Context) {
        if (!isPerformanceTest(intent, context)) {
            return
        }

        disableOnboarding(context)
        disableTrackingProtectionPopups(context)
    }

    /**
     * The checks for the USB connections and ADB debugging are checks in case another application
     * tries to leverage this intent to trigger a code path for Firefox that shouldn't be used unless
     * it is for testing visual metrics. These checks aren't full proof but most of our users won't have
     * ADB on and USB connected at the same time when running Firefox.
     */
    private fun isPerformanceTest(intent: Intent, context: Context): Boolean {
        if (!intent.getBooleanExtra(EXTRA_IS_PERFORMANCE_TEST, false)) {
            return false
        }
        val batteryStatus = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        batteryStatus?.let {
            val isPhonePlugged = it.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1) ==
                    BatteryManager.BATTERY_PLUGGED_USB
            val isAdbEnabled = AndroidSettings.Global.getInt(
                context.contentResolver,
                AndroidSettings.Global.ADB_ENABLED, 0
            ) == 1
            return isPhonePlugged && isAdbEnabled
        }
        return false
    }

    /**
     * Bypasses the onboarding screen on launch
     */
    private fun disableOnboarding(context: Context) {
        FenixOnboarding(context).finish()
    }

    /**
     * Disables the tracking protection popup. However, TP is still on.
     */
    private fun disableTrackingProtectionPopups(context: Context) {
        Settings.getInstance(context).isOverrideTPPopupsForPerformanceTest = true
    }
}

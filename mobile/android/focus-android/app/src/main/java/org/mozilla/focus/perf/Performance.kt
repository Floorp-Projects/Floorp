/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.perf

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Bundle
import android.provider.Settings as AndroidSettings

/**
 * A collection of objects related to app performance.
 */
object Performance {

    private const val EXTRA_IS_PERFORMANCE_TEST = "performancetest"

    fun processIntentIfPerformanceTest(bundle: Bundle?, context: Context) = isPerformanceTest(bundle, context)

    /**
     * This checks for the charging state and ADB debugging in case another application tries to
     * leverage this intent to trigger a code path for Firefox that shouldn't be used unless
     * it is for testing visual metrics. These checks aren't foolproof but most of our users won't
     * have ADB on and charging at the same time when running Firefox.
     */
    private fun isPerformanceTest(bundle: Bundle?, context: Context): Boolean {
        if (bundle == null || !bundle.getBoolean(EXTRA_IS_PERFORMANCE_TEST, false)) {
            return false
        }

        val batteryStatus = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        batteryStatus?.let {
            // We only run perf tests when the device is connected to USB. However, AC may be reported
            // instead if the device is connected through a USB hub so we check both states.
            val extraPlugged = it.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1)
            val isPhonePlugged = extraPlugged == BatteryManager.BATTERY_PLUGGED_USB ||
                extraPlugged == BatteryManager.BATTERY_PLUGGED_AC

            val isAdbEnabled = AndroidSettings.Global.getInt(
                context.contentResolver,
                AndroidSettings.Global.ADB_ENABLED,
                0,
            ) == 1

            return isPhonePlugged && isAdbEnabled
        }
        return false
    }
}

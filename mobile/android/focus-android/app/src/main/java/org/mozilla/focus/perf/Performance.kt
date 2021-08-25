package org.mozilla.focus.perf

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.provider.Settings as AndroidSettings

object Performance {

    const val TAG = "FenixPerf"
    private const val EXTRA_IS_PERFORMANCE_TEST = "performancetest"

    fun processIntentIfPerformanceTest(intent: Intent, context: Context) {
        if (!isPerformanceTest(intent, context)) {
            return
        }
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

}

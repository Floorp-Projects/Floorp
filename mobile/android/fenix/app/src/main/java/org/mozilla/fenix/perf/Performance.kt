/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import androidx.core.content.ContextCompat
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.utils.ext.registerReceiverCompat
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.onboarding.FenixOnboarding
import android.provider.Settings as AndroidSettings

/**
 * A collection of objects related to app performance.
 */
object Performance {
    const val TAG = "FenixPerf"
    val logger = Logger(TAG)

    private const val EXTRA_IS_PERFORMANCE_TEST = "performancetest"

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
        disableFirstTimePWAPopup(context)
        disableTCPPopup(context)
    }

    /**
     * The checks for the charging state and ADB debugging are checks in case another application
     * tries to leverage this intent to trigger a code path for Firefox that shouldn't be used unless
     * it is for testing visual metrics. These checks aren't full proof but most of our users won't have
     * ADB on and charging at the same time when running Firefox.
     */
    private fun isPerformanceTest(intent: Intent, context: Context): Boolean {
        if (!intent.getBooleanExtra(EXTRA_IS_PERFORMANCE_TEST, false)) {
            return false
        }

        val batteryStatus = context.registerReceiverCompat(
            null,
            IntentFilter(Intent.ACTION_BATTERY_CHANGED),
            ContextCompat.RECEIVER_NOT_EXPORTED,
        )

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
        context.components.settings.isOverrideTPPopupsForPerformanceTest = true
    }

    /**
     * Disables the first time PWA popup.
     */
    private fun disableFirstTimePWAPopup(context: Context) {
        context.components.settings.userKnowsAboutPwas = true
    }

    /**
     * Disables the TCP popup.
     */
    private fun disableTCPPopup(context: Context) {
        context.components.settings.shouldShowTotalCookieProtectionCFR = false
    }
}

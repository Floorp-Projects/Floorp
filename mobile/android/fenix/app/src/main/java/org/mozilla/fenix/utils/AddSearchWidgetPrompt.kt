/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.app.Activity
import android.appwidget.AppWidgetManager
import android.content.ComponentName
import android.os.Build
import org.mozilla.fenix.onboarding.WidgetPinnedReceiver
import org.mozilla.gecko.search.SearchWidgetProvider

/**
 * Displays the "add search widget" prompt for capable devices.
 *
 * @param activity the parent [Activity].
 */
fun showAddSearchWidgetPrompt(activity: Activity) {
    // Requesting to pin app widget is only available for Android 8.0 and above
    if (canShowAddSearchWidgetPrompt()) {
        val appWidgetManager = AppWidgetManager.getInstance(activity)
        val searchWidgetProvider =
            ComponentName(activity, SearchWidgetProvider::class.java)
        if (appWidgetManager.isRequestPinAppWidgetSupported) {
            val successCallback = WidgetPinnedReceiver.getPendingIntent(activity)
            appWidgetManager.requestPinAppWidget(searchWidgetProvider, null, successCallback)
        }
    }
}

/**
 * Checks whether the device is capable of displaying the "add search widget" prompt.
 */
fun canShowAddSearchWidgetPrompt() = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O

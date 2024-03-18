/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchwidget

import android.app.Activity
import android.app.Dialog
import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.content.ComponentName
import android.content.Intent
import android.os.Build
import android.os.Bundle
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import mozilla.components.support.utils.PendingIntentUtils
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.SearchWidget
import org.mozilla.focus.activity.MainActivity
import org.mozilla.focus.ui.theme.FocusTheme

object SearchWidgetUtils {

    private fun addSearchWidgetToHomeScreen(activity: Activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val appWidgetManager = AppWidgetManager.getInstance(activity)
            val searchWidgetProvider = ComponentName(activity, SearchWidgetProvider::class.java)
            if (appWidgetManager!!.isRequestPinAppWidgetSupported) {
                val pinnedWidgetCallbackIntent = Intent(activity, SearchWidgetProvider::class.java)
                val successCallback = PendingIntent.getBroadcast(
                    activity,
                    0,
                    pinnedWidgetCallbackIntent,
                    PendingIntentUtils.defaultFlags or
                        PendingIntent.FLAG_UPDATE_CURRENT,
                )
                appWidgetManager.requestPinAppWidget(searchWidgetProvider, Bundle(), successCallback)
            }
        }
    }

    /**
     * Shows promote search widget dialog
     */
    fun showPromoteSearchWidgetDialog(activity: MainActivity) {
        val promoteSearchWidgetDialog = Dialog(activity)
        promoteSearchWidgetDialog.apply {
            setContentView(
                ComposeView(activity).apply {
                    setViewTreeLifecycleOwner(activity)
                    this.setViewTreeSavedStateRegistryOwner(activity)
                    setContent {
                        FocusTheme {
                            PromoteSearchWidgetDialogCompose(
                                onAddSearchWidgetButtonClick = {
                                    addSearchWidgetToHomeScreen(activity)
                                    SearchWidget.addToHomeScreenButton.record(NoExtras())
                                },
                                onDismiss = {
                                    promoteSearchWidgetDialog.dismiss()
                                },
                            )
                        }
                    }
                    isTransitionGroup = true
                },
            )
        }.show()
        SearchWidget.promoteDialogShown.record(NoExtras())
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import mozilla.components.support.utils.PendingIntentUtils
import org.mozilla.fenix.onboarding.view.OnboardingScreen

/**
 * Receiver required to catch callback from Launcher when prompted
 * to add search widget from Onboarding.
 */
class WidgetPinnedReceiver : BroadcastReceiver() {

    companion object {
        const val ACTION = "org.mozilla.fenix.onboarding.WidgetPinnedReceiver.PIN_SEARCH_WIDGET_SUCCESS"

        /**
         * Prepare success callback for when requesting to pin Search Widget.
         */
        fun getPendingIntent(context: Context): PendingIntent {
            val callbackIntent = Intent(context, WidgetPinnedReceiver::class.java)
            val bundle = Bundle()
            bundle.putInt(AppWidgetManager.EXTRA_APPWIDGET_ID, 1)
            callbackIntent.putExtras(bundle)
            return PendingIntent.getBroadcast(
                context,
                0,
                callbackIntent,
                PendingIntentUtils.defaultFlags or PendingIntent.FLAG_UPDATE_CURRENT,
            )
        }
    }

    /**
     * Object containing boolean that updates behavior of Add Search Widget
     * card from [OnboardingScreen].
     * - True if widget added successfully and app resumed from launcher add widget dialog.
     * - False if dialog opened but widget was not added.
     */
    object WidgetPinnedState {
        private val _isPinned = MutableStateFlow(false)
        val isPinned: StateFlow<Boolean> = _isPinned

        /**
         * Update state when resumed to add search widget card
         * and the widget was added successfully.
         */
        fun widgetPinned() {
            _isPinned.value = true
        }
    }

    override fun onReceive(context: Context?, intent: Intent?) {
        if (context == null || intent == null) {
            return
        } else if (intent.action == ACTION) {
            // Returned to fragment, go to next page and update button behavior.
            WidgetPinnedState.widgetPinned()
        }

        val widgetId = intent.getIntExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, -1)

        if (widgetId == -1) {
            // No widget id received.
            return
        } else {
            // Callback from system, widget pinned successfully, update compose now.
            val updateIntent = Intent(ACTION)
            LocalBroadcastManager.getInstance(context).sendBroadcast(updateIntent)
        }
    }
}

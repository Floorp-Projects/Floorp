/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.support.utils.SafeIntent

/**
 * Tracks how the application was opened through [Event.AppOpenedAllSourceStartup].
 * We only considered to be "opened" if it received an intent and the app was in the background.
 */
class AppAllSourceStartTelemetry(private val metrics: MetricController) : LifecycleObserver {

    // default value is true to capture the first launch of the application
    private var wasApplicationInBackground = true

    init {
        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
    }

    fun receivedIntentInExternalAppBrowserActivity(safeIntent: SafeIntent) {
        setAppOpenedAllSourceFromIntent(safeIntent, true)
    }

    fun receivedIntentInHomeActivity(safeIntent: SafeIntent) {
        setAppOpenedAllSourceFromIntent(safeIntent, false)
    }

    private fun setAppOpenedAllSourceFromIntent(intent: SafeIntent, isExternalAppBrowserActivity: Boolean) {
        if (!wasApplicationInBackground) {
            return
        }

        val source = when {
            isExternalAppBrowserActivity -> Event.AppOpenedAllSourceStartup.Source.CUSTOM_TAB
            intent.isLauncherIntent -> Event.AppOpenedAllSourceStartup.Source.APP_ICON
            intent.action == Intent.ACTION_VIEW -> Event.AppOpenedAllSourceStartup.Source.LINK
            else -> Event.AppOpenedAllSourceStartup.Source.UNKNOWN
        }

        metrics.track(Event.AppOpenedAllSourceStartup(source))

        wasApplicationInBackground = false
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    @VisibleForTesting(otherwise = PRIVATE)
    fun onApplicationOnStop() {
        wasApplicationInBackground = true
    }
}

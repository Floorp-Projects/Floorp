/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.support.utils.SafeIntent
import org.mozilla.fenix.components.metrics.Event.AppAllStartup
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source.APP_ICON
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source.CUSTOM_TAB
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source.LINK
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Source.UNKNOWN
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.ERROR
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.HOT
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.COLD
import org.mozilla.fenix.components.metrics.Event.AppAllStartup.Type.WARM

/**
 * Tracks application startup source, type, and whether or not activity has savedInstance to restore
 * the activity from. Sample metric = [source = COLD, type = APP_ICON, hasSavedInstance = false]
 * The basic idea is to collect these metrics from different phases of startup through
 * [AppAllStartup] and finally report them on Activity's onResume() function.
 */
@Suppress("TooManyFunctions")
class AppStartupTelemetry(private val metrics: MetricController) : LifecycleObserver {

    init {
        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
    }

    private var isMetricRecordedSinceAppWasForegrounded = false
    private var wasAppCreateCalledBeforeActivityCreate = false

    private var onCreateData: AppAllStartup? = null
    private var onRestartData: Pair<Type, Boolean?>? = null
    private var onNewIntentData: Source? = null

    fun onFenixApplicationOnCreate() {
        wasAppCreateCalledBeforeActivityCreate = true
    }

    fun onHomeActivityOnCreate(safeIntent: SafeIntent, hasSavedInstanceState: Boolean) {
        setOnCreateData(safeIntent, hasSavedInstanceState, false)
    }

    fun onExternalAppBrowserOnCreate(safeIntent: SafeIntent, hasSavedInstanceState: Boolean) {
        setOnCreateData(safeIntent, hasSavedInstanceState, true)
    }

    fun onHomeActivityOnRestart() {
        // we are not setting [Source] in this method since source is derived from an intent.
        // therefore source gets set in onNewIntent().
        onRestartData = Pair(HOT, null)
    }

    fun onHomeActivityOnNewIntent(safeIntent: SafeIntent) {
        // we are only setting [Source] in this method since source is derived from an intent].
        // other metric fields are set in onRestart()
        onNewIntentData = getStartupSourceFromIntent(safeIntent, false)
    }

    private fun setOnCreateData(
        safeIntent: SafeIntent,
        hasSavedInstanceState: Boolean,
        isExternalAppBrowserActivity: Boolean
    ) {
        onCreateData = AppAllStartup(
            getStartupSourceFromIntent(safeIntent, isExternalAppBrowserActivity),
            getAppStartupType(),
            hasSavedInstanceState
        )
        wasAppCreateCalledBeforeActivityCreate = false
    }

    private fun getAppStartupType(): Type {
        return if (wasAppCreateCalledBeforeActivityCreate) COLD else WARM
    }

    private fun getStartupSourceFromIntent(
        intent: SafeIntent,
        isExternalAppBrowserActivity: Boolean
    ): Source {
        return when {
            // since the intent action is same (ACTION_VIEW) for both CUSTOM_TAB and LINK.
            // we have to make sure that we are checking for CUSTOM_TAB condition first as this
            // check does not rely on intent action
            isExternalAppBrowserActivity -> CUSTOM_TAB
            intent.isLauncherIntent -> APP_ICON
            intent.action == Intent.ACTION_VIEW -> LINK
            // one of the unknown case is app switcher, where we go to the recent tasks to launch
            // Fenix.
            else -> UNKNOWN
        }
    }

    /**
     * The reason we record metric on resume is because we need to wait for onNewIntent(), and
     * we are not guaranteed that onNewIntent() will be called before or after onStart() / onRestart().
     * However we are guaranteed onResume() will be called after onNewIntent() and  onStart(). Source:
     * https://developer.android.com/reference/android/app/Activity#onNewIntent(android.content.Intent)
     */
    fun onHomeActivityOnResume() {
        recordMetric()
    }

    private fun recordMetric() {
        if (!isMetricRecordedSinceAppWasForegrounded) {
            val appAllStartup: AppAllStartup = if (onCreateData != null) {
                onCreateData!!
            } else {
                mergeOnRestartAndOnNewIntentIntoStartup()
            }
            metrics.track(appAllStartup)
            isMetricRecordedSinceAppWasForegrounded = true
        }
        // we don't want any weird previous states to persist on our next metric record.
        onCreateData = null
        onNewIntentData = null
        onRestartData = null
    }

    private fun mergeOnRestartAndOnNewIntentIntoStartup(): AppAllStartup {
        return AppAllStartup(
            onNewIntentData ?: UNKNOWN,
            onRestartData?.first ?: ERROR,
            onRestartData?.second
        )
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun onApplicationOnStop() {
        // application was backgrounded, we need to record the new metric type if
        // application was to come to foreground again.
        // Therefore we set the isMetricRecorded flag to false.
        isMetricRecordedSinceAppWasForegrounded = false
    }
}

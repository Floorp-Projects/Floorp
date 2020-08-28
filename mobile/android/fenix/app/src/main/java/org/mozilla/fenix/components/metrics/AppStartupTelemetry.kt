/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Intent
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.core.view.doOnPreDraw
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import kotlinx.coroutines.runBlocking
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
import java.lang.reflect.Modifier.PRIVATE

/**
 * Tracks application startup source, type, launch time, and whether or not activity has
 * savedInstance to restore the activity from.
 * Sample = [source = COLD, type = APP_ICON, hasSavedInstanceState = false,launchTimeNanoSeconds = 1824000000]
 * The basic idea is to collect these metrics from different phases of startup through
 * [AppAllStartup] and finally report them on Activity's onResume() function.
 */
@Suppress("TooManyFunctions")
class AppStartupTelemetry(
    private val metrics: MetricController,
    @VisibleForTesting(otherwise = PRIVATE)
    var appLaunchTimeMeasurement: AppLaunchTimeMeasurement = AppLaunchTimeMeasurement()
) : LifecycleObserver {

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

    fun onHomeActivityOnCreate(
        safeIntent: SafeIntent,
        hasSavedInstanceState: Boolean,
        homeActivityInitTimeStampNanoSeconds: Long,
        rootContainer: View
    ) {
        setOnCreateData(safeIntent, hasSavedInstanceState, homeActivityInitTimeStampNanoSeconds, false)
        rootContainer.doOnPreDraw {
            onPreDraw()
        }
    }

    fun onExternalAppBrowserOnCreate(
        safeIntent: SafeIntent,
        hasSavedInstanceState: Boolean,
        homeActivityInitTimeStampNanoSeconds: Long,
        rootContainer: View
    ) {
        setOnCreateData(safeIntent, hasSavedInstanceState, homeActivityInitTimeStampNanoSeconds, true)
        rootContainer.doOnPreDraw {
            onPreDraw()
        }
    }

    fun onHomeActivityOnRestart(rootContainer: View) {
        // DO NOT MOVE ANYTHING ABOVE THIS..
        // we are measuring startup time for hot startup type
        appLaunchTimeMeasurement.onHomeActivityOnRestart()

        // we are not setting [Source] in this method since source is derived from an intent.
        // therefore source gets set in onNewIntent().
        onRestartData = Pair(HOT, null)

        rootContainer.doOnPreDraw {
            onPreDraw()
        }
    }

    fun onHomeActivityOnNewIntent(safeIntent: SafeIntent) {
        // we are only setting [Source] in this method since source is derived from an intent].
        // other metric fields are set in onRestart()
        onNewIntentData = getStartupSourceFromIntent(safeIntent, false)
    }

    private fun setOnCreateData(
        safeIntent: SafeIntent,
        hasSavedInstanceState: Boolean,
        homeActivityInitTimeStampNanoSeconds: Long,
        isExternalAppBrowserActivity: Boolean
    ) {
        onCreateData = AppAllStartup(
            getStartupSourceFromIntent(safeIntent, isExternalAppBrowserActivity),
            getAppStartupType(),
            hasSavedInstanceState
        )
        appLaunchTimeMeasurement.onHomeActivityOnCreate(homeActivityInitTimeStampNanoSeconds)
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

    private suspend fun recordMetric() {
        if (!isMetricRecordedSinceAppWasForegrounded) {
            val appAllStartup: AppAllStartup = if (onCreateData != null) {
                onCreateData!!
            } else {
                mergeOnRestartAndOnNewIntentIntoStartup()
            }
            appAllStartup.launchTime = appLaunchTimeMeasurement.getApplicationLaunchTime(appAllStartup.type)
            metrics.track(appAllStartup)
            isMetricRecordedSinceAppWasForegrounded = true
        }
        // we don't want any weird previous states to persist on our next metric record.
        onCreateData = null
        onNewIntentData = null
        onRestartData = null
        appLaunchTimeMeasurement = AppLaunchTimeMeasurement()
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

    /**
     *record the timestamp for the first frame drawn
     */
    @VisibleForTesting(otherwise = PRIVATE)
    fun onPreDraw() {
        // DO NOT MOVE ANYTHING ABOVE THIS..
        // we are measuring startup time here.
        appLaunchTimeMeasurement.onFirstFramePreDraw()
    }

    /**
     * record the metrics, blocking the main thread to make sure we get our metrics recorded before
     * the application potentially closes.
     */
    fun onStop() {
        runBlocking {
            recordMetric()
        }
    }
}

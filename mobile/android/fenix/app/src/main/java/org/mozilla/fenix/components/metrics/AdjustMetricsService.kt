/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.app.Activity
import android.app.Application
import android.os.Bundle
import android.util.Log
import com.adjust.sdk.Adjust
import com.adjust.sdk.AdjustConfig
import com.adjust.sdk.AdjustEvent
import com.adjust.sdk.LogLevel
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.crash.CrashReporter
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.FirstSession
import org.mozilla.fenix.ext.settings

class AdjustMetricsService(
    private val application: Application,
    private val storage: MetricsStorage,
    private val crashReporter: CrashReporter,
    private val dispatcher: CoroutineDispatcher = Dispatchers.IO,
) : MetricsService {
    override val type = MetricServiceType.Marketing

    override fun start() {
        if ((BuildConfig.ADJUST_TOKEN.isNullOrBlank())) {
            Log.i(LOGTAG, "No adjust token defined")

            if (Config.channel.isReleased) {
                throw IllegalStateException("No adjust token defined for release build")
            }

            return
        }

        val config = AdjustConfig(
            application,
            BuildConfig.ADJUST_TOKEN,
            AdjustConfig.ENVIRONMENT_PRODUCTION,
            true,
        )
        config.setPreinstallTrackingEnabled(true)

        val installationPing = FirstSessionPing(application)

        val timerId = FirstSession.adjustAttributionTime.start()
        config.setOnAttributionChangedListener {
            FirstSession.adjustAttributionTime.stopAndAccumulate(timerId)
            if (!it.network.isNullOrEmpty()) {
                application.applicationContext.settings().adjustNetwork =
                    it.network
            }
            if (!it.adgroup.isNullOrEmpty()) {
                application.applicationContext.settings().adjustAdGroup =
                    it.adgroup
            }
            if (!it.creative.isNullOrEmpty()) {
                application.applicationContext.settings().adjustCreative =
                    it.creative
            }
            if (!it.campaign.isNullOrEmpty()) {
                application.applicationContext.settings().adjustCampaignId =
                    it.campaign
            }

            installationPing.checkAndSend()
        }

        config.setLogLevel(LogLevel.SUPRESS)
        Adjust.onCreate(config)
        Adjust.setEnabled(true)
        application.registerActivityLifecycleCallbacks(AdjustLifecycleCallbacks())
    }

    override fun stop() {
        Adjust.setEnabled(false)
        Adjust.gdprForgetMe(application.applicationContext)
    }

    @Suppress("TooGenericExceptionCaught")
    override fun track(event: Event) {
        CoroutineScope(dispatcher).launch {
            try {
                if (event is Event.GrowthData) {
                    if (storage.shouldTrack(event)) {
                        Adjust.trackEvent(AdjustEvent(event.tokenName))
                        storage.updateSentState(event)
                    } else {
                        storage.updatePersistentState(event)
                    }
                }
            } catch (e: Exception) {
                crashReporter.submitCaughtException(e)
            }
        }
    }

    override fun shouldTrack(event: Event): Boolean =
        event is Event.GrowthData

    companion object {
        private const val LOGTAG = "AdjustMetricsService"
    }

    private class AdjustLifecycleCallbacks : Application.ActivityLifecycleCallbacks {
        override fun onActivityResumed(activity: Activity) {
            Adjust.onResume()
        }

        override fun onActivityPaused(activity: Activity) {
            Adjust.onPause()
        }

        override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) { /* noop */ }

        override fun onActivityStarted(activity: Activity) { /* noop */ }

        override fun onActivityStopped(activity: Activity) { /* noop */ }

        override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) { /* noop */ }

        override fun onActivityDestroyed(activity: Activity) { /* noop */ }
    }
}

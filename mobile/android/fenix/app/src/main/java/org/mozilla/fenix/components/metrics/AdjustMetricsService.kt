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
import com.adjust.sdk.LogLevel
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.Config
import org.mozilla.fenix.ext.settings

class AdjustMetricsService(private val application: Application) : MetricsService {
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

        val installationPing = FirstSessionPing(application)

        config.setOnAttributionChangedListener {
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

    // We're not currently sending events directly to Adjust
    override fun track(event: Event) { /* noop */ }
    override fun shouldTrack(event: Event): Boolean = false

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

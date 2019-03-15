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
import java.lang.IllegalStateException

class AdjustMetricsService(private val application: Application) : MetricsService {
    override fun start() {
        if ((BuildConfig.ADJUST_TOKEN.isNullOrEmpty())) {
            Log.i(LOGTAG, "No adjust token defined")

            if (!BuildConfig.DEBUG) {
                throw IllegalStateException("No adjust token defined for release build")
            }

            return
        }

        val config = AdjustConfig(
            application,
            BuildConfig.ADJUST_TOKEN,
            AdjustConfig.ENVIRONMENT_PRODUCTION,
            true
        )

        config.setLogLevel(LogLevel.SUPRESS)

        Adjust.onCreate(config)

        application.registerActivityLifecycleCallbacks(AdjustLifecycleCallbacks())
    }

    // We're not currently sending events directly to Adjust
    override fun track(event: Event) { }
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

        override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle) {}

        override fun onActivityStarted(activity: Activity) {}

        override fun onActivityStopped(activity: Activity) {}

        override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) {}

        override fun onActivityDestroyed(activity: Activity) {}
    }
}

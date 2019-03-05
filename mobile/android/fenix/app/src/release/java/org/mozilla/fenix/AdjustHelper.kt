/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.app.Application
import android.os.Bundle
import android.text.TextUtils

import com.adjust.sdk.Adjust
import com.adjust.sdk.AdjustConfig
import com.adjust.sdk.LogLevel
import mozilla.components.service.glean.Glean

object AdjustHelper {
    @Suppress("UnreachableCode")
    fun setupAdjustIfNeeded(application: FenixApplication) {
        // RELEASE: Enable Adjust - This class has different implementations for all build types.
        return

        if (TextUtils.isEmpty(BuildConfig.ADJUST_TOKEN)) {
            throw IllegalStateException("No adjust token defined for release build")
        }

        if (!Glean.getUploadEnabled()) {
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

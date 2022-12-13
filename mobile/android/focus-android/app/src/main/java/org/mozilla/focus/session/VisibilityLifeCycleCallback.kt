/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.session

import android.app.Activity
import android.app.ActivityManager
import android.app.Application.ActivityLifecycleCallbacks
import android.content.ComponentCallbacks2
import android.content.Context
import android.content.res.Configuration
import android.os.Bundle
import org.mozilla.focus.FocusApplication
import org.mozilla.focus.appreview.AppReviewUtils.Companion.addAppOpenings

/**
 * This ActivityLifecycleCallbacks implementations tracks if there is at least one activity in the
 * STARTED state (meaning some part of our application is visible).
 * Based on this information the current task can be removed if the app is not visible.
 */
@Suppress("TooManyFunctions", "EmptyFunctionBlock")
class VisibilityLifeCycleCallback(private val context: Context) : ActivityLifecycleCallbacks, ComponentCallbacks2 {
    /**
     * Activities are not stopped/started in an ordered way. So we are using
     */
    private var activitiesInStartedState = 0
    private var appInForeground = false
    private fun finishAndRemoveTaskIfInBackground() {
        if (activitiesInStartedState == 0) {
            val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
            for (task in activityManager.appTasks) {
                task.finishAndRemoveTask()
            }
        }
    }

    override fun onActivityStarted(activity: Activity) {
        activitiesInStartedState++
    }

    override fun onActivityStopped(activity: Activity) {
        activitiesInStartedState--
    }

    override fun onActivityResumed(activity: Activity) {}
    override fun onActivityPaused(activity: Activity) {}
    override fun onActivityCreated(activity: Activity, bundle: Bundle?) {
        if (!appInForeground) {
            appInForeground = true
            addAppOpenings(context)
        }
    }

    override fun onActivitySaveInstanceState(activity: Activity, bundle: Bundle) {}
    override fun onActivityDestroyed(activity: Activity) {}
    override fun onTrimMemory(level: Int) {
        if (level == ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN) {
            appInForeground = false
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {}
    override fun onLowMemory() {}

    companion object {
        /**
         * If all activities of this app are in the background then finish and remove all tasks. After
         * that the app won't show up in "recent apps" anymore.
         */
        fun finishAndRemoveTaskIfInBackground(context: Context) {
            (context.applicationContext as FocusApplication)
                .visibilityLifeCycleCallback
                ?.finishAndRemoveTaskIfInBackground()
        }

        fun isInBackground(context: Context): Boolean {
            return (context.applicationContext as FocusApplication)
                .visibilityLifeCycleCallback?.activitiesInStartedState == 0
        }
    }
}

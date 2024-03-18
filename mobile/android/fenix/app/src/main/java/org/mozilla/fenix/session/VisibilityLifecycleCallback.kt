/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import android.app.Activity
import android.app.ActivityManager
import android.app.Application
import android.content.Context
import android.os.Bundle
import mozilla.components.browser.state.selector.privateTabs
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.ext.components

/**
 * This ActivityLifecycleCallbacks implementations tracks if there is at least one activity in the
 * STARTED state (meaning some part of our application is visible).
 * Based on this information the current task can be removed if the app is not visible.
 */
@SuppressWarnings("EmptyFunctionBlock")
class VisibilityLifecycleCallback(private val activityManager: ActivityManager?) :
    Application.ActivityLifecycleCallbacks {

    /**
     * Activities are not stopped/started in an ordered way. So we are using
     */
    private var activitiesInStartedState: Int = 0

    /**
     * Finishes and removes the list of AppTasks only if the application is in the background.
     * The application is considered to be in the background if it has at least 1 Activity in the
     * started state
     * @return True if application is in background (also finishes and removes all AppTasks),
     *          false otherwise
     */
    private fun finishAndRemoveTaskIfInBackground(): Boolean {
        if (activitiesInStartedState == 0) {
            activityManager?.let {
                for (task in it.appTasks) {
                    task.finishAndRemoveTask()
                }
                return true
            }
        }
        return false
    }

    override fun onActivityStarted(activity: Activity) {
        activitiesInStartedState++
    }

    override fun onActivityStopped(activity: Activity) {
        activitiesInStartedState--
        if (activitiesInStartedState == 0) {
            removeTCPException(activity)
        }
    }

    override fun onActivityResumed(activity: Activity) {}

    override fun onActivityPaused(activity: Activity) {}

    override fun onActivityCreated(activity: Activity, bundle: Bundle?) {}

    override fun onActivitySaveInstanceState(activity: Activity, bundle: Bundle) {}

    override fun onActivityDestroyed(activity: Activity) {}

    /**
     * For private tabs, set the tracking protection exception
     * to the default state if the app is closed.
     */
    private fun removeTCPException(activity: Activity) {
        activity.components.core.store.state.privateTabs.filter {
            it.trackingProtection.ignoredOnTrackingProtection
        }.forEach { privateTab ->
            activity.components.useCases.trackingProtectionUseCases.removeException(privateTab.id)
        }
    }

    companion object {
        /**
         * If all activities of this app are in the background then finish and remove all tasks. After
         * that the app won't show up in "recent apps" anymore.
         *
         * @return True if application is in background (and consequently, finishes and removes all tasks),
         *          false otherwise.
         */
        internal fun finishAndRemoveTaskIfInBackground(context: Context): Boolean {
            return (context.applicationContext as FenixApplication)
                .visibilityLifecycleCallback?.finishAndRemoveTaskIfInBackground() ?: false
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import android.app.Activity
import android.app.Application
import android.os.Bundle
import mozilla.components.support.utils.RunWhenReadyQueue
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.settings.account.AuthIntentReceiverActivity
import org.mozilla.fenix.widget.VoiceSearchActivity

/**
 * These callbacks handle binding performance code to the activity lifecycle.
 */
@SuppressWarnings("EmptyFunctionBlock")
class PerformanceActivityLifecycleCallbacks(
    private val visualCompletenessQueue: RunWhenReadyQueue,
) : Application.ActivityLifecycleCallbacks {

    override fun onActivityCreated(activity: Activity, bundle: Bundle?) {
        ifNecessaryPostVisualCompleteness(activity)
    }

    /**
     * Returns true if it is a terminal activity, or false if it
     * an activity used in transition.
     */
    private fun isTransientActivity(activity: Activity): Boolean {
        // These are the current list of non terminal activites.
        // They have been allow-listed in case new activities are added to the application
        // to ensure these new activities would not crash the application.
        return isTransientActivityInMigrationVariant(activity) ||
            (activity is IntentReceiverActivity) ||
            (activity is VoiceSearchActivity) ||
            (activity is AuthIntentReceiverActivity)
    }

    /**
     * This marks visualCompletenessQueue as ready, either delayed or right away, if the activity is a
     * terminal activity. If not, do nothing.
     */
    private fun ifNecessaryPostVisualCompleteness(activity: Activity) {
        fun shouldStartVisualCompletenessQueueImmediately(): Boolean {
            return !isTransientActivity(activity)
        }

        if (activity is HomeActivity) {
            // We should delay the visualCompletenessQueue when reaching the HomeActivity
            // to ensure all tasks are delayed until after visual completeness
            activity.setVisualCompletenessQueueReady()
        } else if (shouldStartVisualCompletenessQueueImmediately()) {
            // If we do not go through the home activity, we have to start the tasks
            // immediately to avoid spending time implementing it.
            visualCompletenessQueue.ready()
        }
    }

    override fun onActivityStarted(activity: Activity) {}
    override fun onActivityStopped(activity: Activity) {}
    override fun onActivityResumed(activity: Activity) {}
    override fun onActivityPaused(activity: Activity) {}
    override fun onActivitySaveInstanceState(activity: Activity, bundle: Bundle) {}
    override fun onActivityDestroyed(activity: Activity) {}

    companion object {
        /**
         * The source files are different from migration and non migration variants.
         * We use this property to extend the [isTransientActivity] implementation for the
         * migration variants.
         */
        var isTransientActivityInMigrationVariant: (Activity) -> Boolean = { false }
    }
}

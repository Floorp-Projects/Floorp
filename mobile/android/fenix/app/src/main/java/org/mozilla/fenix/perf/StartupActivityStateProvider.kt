/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.Activity
import android.app.Application
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.GleanMetrics.Metrics
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.android.DefaultActivityLifecycleCallbacks

private val logger = Logger("StartupActivityState")

/**
 * Provides meta information about the Activities that occur during the initial parts of start up
 * and their state.
 *
 * [registerInAppOnCreate] must be called for this class to work correctly.
 */
class StartupActivityStateProvider {

    enum class FirstForegroundActivity {
        TO_BE_DETERMINED,
        HOME_ACTIVITY,
        UNKNOWN
    }

    enum class FirstForegroundActivityState {
        BEFORE_FOREGROUND,
        CURRENTLY_FOREGROUNDED,
        AFTER_FOREGROUND,
    }

    /** The first [Activity] that has been foreground in this process lifetime. */
    var firstForegroundActivityOfProcess = FirstForegroundActivity.TO_BE_DETERMINED
        private set

    /** The current foreground state of the [firstForegroundActivityOfProcess]. */
    var firstForegroundActivityState = FirstForegroundActivityState.BEFORE_FOREGROUND
        private set

    /**
     * Registers the handlers needed by this class: this is expected to be called from
     * [Application.onCreate].
     */
    fun registerInAppOnCreate(application: Application) {
        application.registerActivityLifecycleCallbacks(StateActivityLifecycleCallbacks())
    }

    private inner class StateActivityLifecycleCallbacks : DefaultActivityLifecycleCallbacks {
        override fun onActivityStarted(activity: Activity) {
            if (firstForegroundActivityOfProcess != FirstForegroundActivity.TO_BE_DETERMINED) {
                // This should never happen because we remove the listener in onStop and old activities
                // should be stopped before new ones are started but the call order may change slightly
                // between devices.
                Metrics.activityStateProviderError.set(true)
                logger.error("StartupActivityStateProvider...onActivityStarted unexpectedly called twice.")
                return
            }

            firstForegroundActivityOfProcess = when (activity) {
                is HomeActivity -> FirstForegroundActivity.HOME_ACTIVITY
                else -> FirstForegroundActivity.UNKNOWN
            }

            firstForegroundActivityState = FirstForegroundActivityState.CURRENTLY_FOREGROUNDED
        }

        override fun onActivityStopped(activity: Activity) {
            firstForegroundActivityState = FirstForegroundActivityState.AFTER_FOREGROUND

            activity.application.unregisterActivityLifecycleCallbacks(this) // no more state updates needed.
        }
    }
}

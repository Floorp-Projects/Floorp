/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.scheduler

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.OnLifecycleEvent
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.metrics.Baseline

/**
 * Connects process lifecycle events from Android to Glean's handleEvent
 * functionality (where the actual work of sending pings is done).
 */
internal class GleanLifecycleObserver : LifecycleObserver {
    /**
     * Calls the "background" event when entering the background.
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onEnterBackground() {
        Glean.handleEvent(Glean.PingEvent.Background)
    }

    /**
     * Updates the baseline.sessions metric when entering the foreground.
     * We use ON_RESUME here because there are a number of paths by which
     * the application can re-enter the foreground, e.g. from a cold start
     * or warm start, etc., all of which eventually call ON_RESUME.
     *
     * https://developer.android.com/reference/android/app/Activity.html#onResume()
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun onEnterForeground() {
        Baseline.sessions.add()
    }
}

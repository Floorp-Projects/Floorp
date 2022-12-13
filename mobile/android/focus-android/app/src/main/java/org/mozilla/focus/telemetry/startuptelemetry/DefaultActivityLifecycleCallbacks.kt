/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry.startuptelemetry

import android.app.Activity
import android.app.Application
import android.os.Bundle

/**
 * An inheritance of [Application.ActivityLifecycleCallbacks] where each method has a default
 * implementation that does nothing. This allows classes that extend this interface to have
 * more concise definitions if they don't implement some methods; this is in the spirit of
 * other `Default*` classes, such as [androidx.lifecycle.DefaultLifecycleObserver].
 */
interface DefaultActivityLifecycleCallbacks : Application.ActivityLifecycleCallbacks {
    override fun onActivityCreated(activity: Activity, bundle: Bundle?) {}
    override fun onActivityStarted(activity: Activity) {}
    override fun onActivityResumed(activity: Activity) {}
    override fun onActivityPaused(activity: Activity) {}
    override fun onActivityStopped(activity: Activity) {}
    override fun onActivitySaveInstanceState(activity: Activity, bundle: Bundle) {}
    override fun onActivityDestroyed(activity: Activity) {}
}

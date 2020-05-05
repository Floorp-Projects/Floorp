/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.os.Bundle
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.base.crash.Breadcrumb

/**
 * Records breadcrumbs when the fragment changes.
 *
 * Should be registered as a [LifecycleObserver] on an activity if telemetry is enabled.
 * It will automatically be removed when the lifecycle owner is destroyed.
 */
class BreadcrumbsRecorder(
    private val crashReporter: CrashReporter,
    private val navController: NavController,
    private val getBreadcrumbMessage: (NavDestination) -> String
) : NavController.OnDestinationChangedListener, LifecycleObserver {

    @OnLifecycleEvent(Lifecycle.Event.ON_CREATE)
    fun onCreate() {
        navController.addOnDestinationChangedListener(this)
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    fun onDestroy() {
        navController.removeOnDestinationChangedListener(this)
    }

    /**
     * When the destination changes, record the new destination as a breadcrumb.
     */
    override fun onDestinationChanged(
        controller: NavController,
        destination: NavDestination,
        arguments: Bundle?
    ) {
        crashReporter.recordCrashBreadcrumb(
            Breadcrumb(
                message = getBreadcrumbMessage(destination),
                category = "DestinationChanged",
                level = Breadcrumb.Level.INFO
            )
        )
    }
}

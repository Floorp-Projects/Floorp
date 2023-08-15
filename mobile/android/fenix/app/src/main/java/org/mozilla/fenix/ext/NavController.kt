/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.annotation.SuppressLint
import androidx.annotation.IdRes
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.base.log.logger.Logger
import java.lang.IllegalArgumentException

/**
 * Navigate from the fragment with [id] using the given [directions].
 * If the id doesn't match the current destination, an error is recorded.
 */
fun NavController.nav(@IdRes id: Int?, directions: NavDirections, navOptions: NavOptions? = null) {
    if (id == null || this.currentDestination?.id == id) {
        this.navigate(directions, navOptions)
    } else {
        Logger.error("Fragment id ${this.currentDestination?.id} did not match expected $id")
    }
}

fun NavController.alreadyOnDestination(@IdRes destId: Int?): Boolean {
    return destId?.let { currentDestination?.id == it || popBackStack(it, false) } ?: false
}

fun NavController.navigateSafe(
    @IdRes resId: Int,
    directions: NavDirections,
) {
    if (currentDestination?.id == resId) {
        this.navigate(directions)
    }
}

/**
 * Navigates using the given [directions], and submit a Breadcrumb
 * when an [IllegalArgumentException] happens.
 */
fun NavController.navigateWithBreadcrumb(
    directions: NavDirections,
    navigateFrom: String,
    navigateTo: String,
    crashReporter: CrashReporter,
) {
    try {
        this.navigate(directions)
    } catch (e: IllegalArgumentException) {
        crashReporter.recordCrashBreadcrumb(
            Breadcrumb(
                "Navigation - " +
                    "where we are: $currentDestination," + "where we are going: $navigateTo, " +
                    "where we thought we were: $navigateFrom",
            ),
        )
    }
}

/**
 * Checks if the Fragment with a [fragmentClassName] is on top of the back queue.
 */
@SuppressLint("RestrictedApi")
fun NavController.hasTopDestination(fragmentClassName: String): Boolean {
    return this.currentBackStackEntry?.destination?.displayName?.contains(
        fragmentClassName,
        true,
    ) == true
}

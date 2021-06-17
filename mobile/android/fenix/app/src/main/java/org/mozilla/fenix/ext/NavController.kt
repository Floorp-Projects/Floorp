/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.annotation.IdRes
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.sentry.Sentry
import org.mozilla.fenix.components.isSentryEnabled

/**
 * Navigate from the fragment with [id] using the given [directions].
 * If the id doesn't match the current destination, an error is recorded.
 */
fun NavController.nav(@IdRes id: Int?, directions: NavDirections, navOptions: NavOptions? = null) {
    if (id == null || this.currentDestination?.id == id) {
        this.navigate(directions, navOptions)
    } else {
        recordIdException(this.currentDestination?.id, id)
    }
}

fun NavController.alreadyOnDestination(@IdRes destId: Int?): Boolean {
    return destId?.let { currentDestination?.id == it || popBackStack(it, false) } ?: false
}

fun recordIdException(actual: Int?, expected: Int?) {
    if (isSentryEnabled()) {
        Sentry.capture("Fragment id $actual did not match expected $expected")
    }
}

fun NavController.navigateSafe(
    @IdRes resId: Int,
    directions: NavDirections
) {
    if (currentDestination?.id == resId) {
        this.navigate(directions)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.os.Bundle
import androidx.annotation.IdRes
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.navigation.Navigator
import io.sentry.Sentry
import org.mozilla.fenix.BuildConfig

fun NavController.nav(@IdRes id: Int?, directions: NavDirections) {
    if (id == null || this.currentDestination?.id == id) {
        this.navigate(directions)
    } else {
        recordIdException(this.currentDestination?.id, id)
    }
}

fun NavController.nav(@IdRes id: Int?, directions: NavDirections, extras: Navigator.Extras) {
    if (id == null || this.currentDestination?.id == id) {
        this.navigate(directions, extras)
    } else {
        recordIdException(this.currentDestination?.id, id)
    }
}

fun NavController.nav(@IdRes id: Int?, directions: NavDirections, options: NavOptions) {
    if (id == null || this.currentDestination?.id == id) {
        this.navigate(directions, options)
    } else {
        recordIdException(this.currentDestination?.id, id)
    }
}

fun NavController.nav(
    @IdRes id: Int?,
    @IdRes destId: Int,
    args: Bundle?,
    navOptions: NavOptions?,
    extras: Navigator.Extras?
) {
    if (id == null || this.currentDestination?.id == id) {
        this.navigate(destId, args, navOptions, extras)
    } else {
        recordIdException(this.currentDestination?.id, id)
    }
}

fun NavController.alreadyOnDestination(@IdRes destId: Int?): Boolean {
    return destId?.let { currentDestination?.id == it || popBackStack(it, false) } ?: false
}

fun recordIdException(actual: Int?, expected: Int?) {
    if (!BuildConfig.SENTRY_TOKEN.isNullOrEmpty()) {
        Sentry.capture("Fragment id $actual did not match expected $expected")
    }
}

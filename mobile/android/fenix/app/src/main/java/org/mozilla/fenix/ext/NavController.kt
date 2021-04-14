/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We suppress the calls to `navigate` since we invoke the Android `NavController.navigate` through
// this file. Detekt checks for the `navigate()` function calls, which should be ignored in this file.
@file:Suppress("MozillaNavigateCheck")
package org.mozilla.fenix.ext

import androidx.annotation.IdRes
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.sentry.Sentry
import org.mozilla.fenix.components.isSentryEnabled
import org.mozilla.fenix.perf.NavGraphProvider

/**
 * Navigate from the fragment with [id] using the given [directions].
 * If the id doesn't match the current destination, an error is recorded.
 */
fun NavController.nav(@IdRes id: Int?, directions: NavDirections, navOptions: NavOptions? = null) {
    NavGraphProvider.blockForNavGraphInflation(this)

    if (id == null || this.currentDestination?.id == id) {
        this.navigate(directions, navOptions)
    } else {
        recordIdException(this.currentDestination?.id, id)
    }
}

fun NavController.navigateBlockingForAsyncNavGraph(resId: Int) {
    NavGraphProvider.blockForNavGraphInflation(this)
    this.navigate(resId)
}

fun NavController.navigateBlockingForAsyncNavGraph(directions: NavDirections) {
    NavGraphProvider.blockForNavGraphInflation(this)
    this.navigate(directions)
}

fun NavController.navigateBlockingForAsyncNavGraph(directions: NavDirections, navOptions: NavOptions?) {
    NavGraphProvider.blockForNavGraphInflation(this)
    this.navigate(directions, navOptions)
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
        this.navigateBlockingForAsyncNavGraph(directions)
    }
}

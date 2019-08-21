/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.annotation.IdRes
import androidx.annotation.StringRes
import androidx.fragment.app.Fragment
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.navigation.Navigator
import androidx.navigation.fragment.NavHostFragment.findNavController
import org.mozilla.fenix.components.Components

/**
 * Get the requireComponents of this application.
 */
val Fragment.requireComponents: Components
    get() = requireContext().components

fun Fragment.nav(@IdRes id: Int?, directions: NavDirections) {
    findNavController(this).nav(id, directions)
}

fun Fragment.nav(@IdRes id: Int?, directions: NavDirections, extras: Navigator.Extras) {
    findNavController(this).nav(id, directions, extras)
}

fun Fragment.nav(@IdRes id: Int?, directions: NavDirections, options: NavOptions) {
    findNavController(this).nav(id, directions, options)
}

fun Fragment.getPreferenceKey(@StringRes resourceId: Int): String = getString(resourceId)

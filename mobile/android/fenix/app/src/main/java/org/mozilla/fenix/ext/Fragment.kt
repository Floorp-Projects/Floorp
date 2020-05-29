/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.annotation.IdRes
import androidx.annotation.StringRes
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.navigation.Navigator
import androidx.navigation.fragment.NavHostFragment.findNavController
import androidx.navigation.fragment.findNavController
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
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

/**
 * Displays the activity toolbar with the given [title].
 * Throws if the fragment is not attached to an [AppCompatActivity].
 */
fun Fragment.showToolbar(title: String) {
    (requireActivity() as AppCompatActivity).title = title
    (activity as HomeActivity).getSupportActionBarAndInflateIfNecessary().show()
}

/**
 * Hides the activity toolbar.
 * Throws if the fragment is not attached to an [AppCompatActivity].
 */
fun Fragment.hideToolbar() {
    (requireActivity() as AppCompatActivity).supportActionBar?.hide()
}

/**
 * Pops the backstack to force users to re-auth if they put the app in the background and return to it
 * while being inside the saved logins flow
 * It also updates the FLAG_SECURE status for the activity's window
 *
 * Does nothing if the user is currently navigating to any of the [destinations] given as a parameter
 *
 */
fun Fragment.redirectToReAuth(destinations: List<Int>, currentDestination: Int?) {
    if (currentDestination !in destinations) {
        activity?.let { it.checkAndUpdateScreenshotPermission(it.settings()) }
        findNavController().popBackStack(R.id.savedLoginsAuthFragment, false)
    }
}

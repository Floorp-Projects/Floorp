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
import androidx.navigation.fragment.NavHostFragment.findNavController
import androidx.navigation.fragment.findNavController
import mozilla.components.concept.base.crash.Breadcrumb
import org.mozilla.fenix.NavHostActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Components

/**
 * Get the requireComponents of this application.
 */
val Fragment.requireComponents: Components
    get() = requireContext().components

fun Fragment.nav(@IdRes id: Int?, directions: NavDirections, options: NavOptions? = null) {
    findNavController(this).nav(id, directions, options)
}

fun Fragment.getPreferenceKey(@StringRes resourceId: Int): String = getString(resourceId)

/**
 * Displays the activity toolbar with the given [title].
 * Throws if the fragment is not attached to an [AppCompatActivity].
 */
fun Fragment.showToolbar(title: String) {
    (requireActivity() as AppCompatActivity).title = title
    (activity as NavHostActivity).getSupportActionBarAndInflateIfNecessary().show()
}

/**
 * Run the [block] only if the [Fragment] is attached.
 *
 * @param block A callback to be executed if the container [Fragment] is attached.
 */
internal inline fun Fragment.runIfFragmentIsAttached(block: () -> Unit) {
    context?.let {
        block()
    }
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
 *
 * Does nothing if the user is currently navigating to any of the [destinations] given as a parameter
 *
 */
fun Fragment.redirectToReAuth(destinations: List<Int>, currentDestination: Int?) {
    if (currentDestination !in destinations) {
        findNavController().popBackStack(R.id.savedLoginsAuthFragment, false)
    }
}

fun Fragment.breadcrumb(
    message: String,
    data: Map<String, String> = emptyMap()
) {
    val activityName = activity?.let { it::class.java.simpleName } ?: "null"

    requireComponents.analytics.crashReporter.recordCrashBreadcrumb(
        Breadcrumb(
            category = this::class.java.simpleName,
            message = message,
            data = data + mapOf(
                "instance" to hashCode().toString(),
                "activityInstance" to activity?.hashCode().toString(),
                "activityName" to activityName
            ),
            level = Breadcrumb.Level.INFO
        )
    )
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import androidx.annotation.StringRes
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import org.mozilla.focus.Components
import org.mozilla.focus.activity.MainActivity

/**
 * Get the components of this application or null if this Fragment is not attached to a Context.
 */
val Fragment.components: Components?
    get() = context?.components

/**
 * Get the components of this application.
 *
 * This method will throw an exception if this Fragment is not attached to a Context.
 */
val Fragment.requireComponents: Components
    get() = requireContext().components

/**
 * Get the preference key.
 * @param preferenceId Resource ID from preference_keys
 */
fun Fragment.getPreferenceKey(@StringRes preferenceId: Int): String = getString(preferenceId)

/**
 * Displays the toolbar with the given [title] if the parent activity
 * can be casted to [AppCompatActivity] and [MainActivity]
 */
fun Fragment.showToolbar(title: String) {
    (requireActivity() as? AppCompatActivity)?.title = title
    (requireActivity() as? MainActivity)?.getToolbar()?.show()
}

/**
 * Hides the activity toolbar if the fragment is attached to an [AppCompatActivity].
 */
fun Fragment.hideToolbar() {
    (requireActivity() as? AppCompatActivity)?.supportActionBar?.hide()
}

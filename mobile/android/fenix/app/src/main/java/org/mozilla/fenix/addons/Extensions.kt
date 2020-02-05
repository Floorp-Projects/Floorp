/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.view.View
import androidx.fragment.app.Fragment
import org.mozilla.fenix.components.FenixSnackbar
import java.text.NumberFormat
import java.util.Locale

/**
 * Get the formatted number amount for the current default locale.
 *
 * @param amount The number of addons to be formatted for the current default locale..
 */
internal fun getFormattedAmount(amount: Int): String {
    return NumberFormat.getNumberInstance(Locale.getDefault()).format(amount)
}

/**
 * Shows the Fenix Snackbar in the given view along with the provided text.
 *
 * @param view A [View] used to determine a parent for the [FenixSnackbar].
 * @param text The text to display in the [FenixSnackbar].
 */
internal fun showSnackBar(view: View, text: String) {
    FenixSnackbar.make(view, FenixSnackbar.LENGTH_SHORT)
        .setText(text)
        .show()
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

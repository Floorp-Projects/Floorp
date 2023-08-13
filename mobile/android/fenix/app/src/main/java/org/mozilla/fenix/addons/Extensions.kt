/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.view.View
import org.mozilla.fenix.components.FenixSnackbar

/**
 * Shows the Fenix Snackbar in the given view along with the provided text.
 *
 * @param view A [View] used to determine a parent for the [FenixSnackbar].
 * @param text The text to display in the [FenixSnackbar].
 * @param duration The duration to show the [FenixSnackbar] for.
 */
internal fun showSnackBar(view: View, text: String, duration: Int = FenixSnackbar.LENGTH_SHORT) {
    FenixSnackbar.make(
        view = view,
        duration = duration,
        isDisplayedWithBrowserToolbar = true,
    )
        .setText(text)
        .show()
}

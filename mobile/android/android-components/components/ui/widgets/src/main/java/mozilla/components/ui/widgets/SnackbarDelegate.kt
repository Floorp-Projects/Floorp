/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets

import android.view.View
import com.google.android.material.snackbar.Snackbar

/**
 * Delegate to display a snackbar.
 */
interface SnackbarDelegate {
    /**
     * Displays a snackbar.
     *
     * @param snackBarParentView The view to find a parent from for displaying the Snackbar.
     * @param text The text to show. Can be formatted text.
     * @param duration How long to display the message.
     * @param action String resource to display for the action.
     * @param listener callback to be invoked when the action is clicked.
     */
    fun show(
        snackBarParentView: View,
        text: Int,
        duration: Int,
        action: Int = 0,
        listener: ((v: View) -> Unit)? = null,
    )
}

/**
 * Default implementation for [SnackbarDelegate]. Will display a standard default Snackbar.
 */
class DefaultSnackbarDelegate : SnackbarDelegate {
    override fun show(
        snackBarParentView: View,
        text: Int,
        duration: Int,
        action: Int,
        listener: ((v: View) -> Unit)?,
    ) {
        val snackbar = Snackbar.make(
            snackBarParentView,
            text,
            duration,
        )

        if (action != 0 && listener != null) {
            snackbar.setAction(action, listener)
        }

        snackbar.show()
    }
}

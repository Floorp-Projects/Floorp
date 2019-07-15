/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.view.View
import androidx.annotation.StringRes
import com.google.android.material.snackbar.Snackbar
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import org.mozilla.fenix.components.FenixSnackbar

class FenixSnackbarDelegate(val view: View, private val anchorView: View?) :
    ContextMenuCandidate.SnackbarDelegate {
    override fun show(
        snackBarParentView: View,
        @StringRes text: Int,
        duration: Int,
        @StringRes action: Int,
        listener: ((v: View) -> Unit)?
    ) {
        val snackbar = FenixSnackbar.make(view, Snackbar.LENGTH_LONG).setText(view.context.getString(text))
        if (listener != null && action != 0) {
            snackbar.setAction(view.context.getString(action)) {
                listener.invoke(view)
            }
        }
        snackbar.anchorView = anchorView
        snackbar.show()
    }
}

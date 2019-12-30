/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.view.View
import androidx.annotation.StringRes
import com.google.android.material.snackbar.Snackbar
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import org.mozilla.fenix.components.BrowserSnackbarPresenter

class FenixSnackbarDelegate(val view: View) :
    ContextMenuCandidate.SnackbarDelegate {
    override fun show(
        snackBarParentView: View,
        @StringRes text: Int,
        duration: Int,
        @StringRes action: Int,
        listener: ((v: View) -> Unit)?
    ) {
        if (listener != null && action != 0) {
            BrowserSnackbarPresenter(view).present(
                text = view.context.getString(text),
                length = Snackbar.LENGTH_LONG,
                actionName = view.context.getString(action),
                action = { listener.invoke(view) }
            )
        } else {
            BrowserSnackbarPresenter(view).present(
                text = view.context.getString(text),
                length = Snackbar.LENGTH_LONG
            )
        }
    }
}

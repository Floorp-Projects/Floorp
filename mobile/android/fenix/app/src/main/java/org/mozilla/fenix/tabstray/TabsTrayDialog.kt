/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.app.Dialog
import android.content.Context
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor

/**
 * Default tabs tray dialog implementation for overriding the default on back pressed.
 */
class TabsTrayDialog(
    context: Context,
    theme: Int,
    private val interactor: () -> BrowserTrayInteractor,
) : Dialog(context, theme) {
    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        if (interactor.invoke().onBackPressed()) {
            return
        }

        dismiss()
    }
}

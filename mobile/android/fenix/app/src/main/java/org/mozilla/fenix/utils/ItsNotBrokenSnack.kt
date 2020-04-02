/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.Context
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView

class ItsNotBrokenSnack(val context: Context) {
    fun showSnackbar(issueNumber: String) {
        val rootView =
            context.getRootView()

        rootView?.let {
            FenixSnackbar.make(
                view = it,
                duration = FenixSnackbar.LENGTH_SHORT,
                isDisplayedOnBrowserFragment = false
            )
                .setText(message.replace("%", issueNumber))
                .setAction("Add Tab to Issue") {
                    context.components.useCases.tabsUseCases.addTab
                        .invoke(issues + issueNumber)
                }
                .show()
        }
    }

    companion object {
        const val issues = "https://github.com/mozilla-mobile/fenix/issues/"
        const val message = "Feature is not implemented, Issue #%"
    }
}

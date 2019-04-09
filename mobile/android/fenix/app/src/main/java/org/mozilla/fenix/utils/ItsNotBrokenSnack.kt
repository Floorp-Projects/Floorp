/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.Context
import android.view.View
import android.view.ViewGroup
import com.google.android.material.snackbar.Snackbar
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components

class ItsNotBrokenSnack(val context: Context) {
    fun showSnackbar(issueNumber: String) {
        val rootView =
            context.asActivity()?.window?.decorView?.findViewById<View>(android.R.id.content) as ViewGroup

        FenixSnackbar.make(rootView, Snackbar.LENGTH_SHORT)
            .setText(message.replace("%", issueNumber))
            .setAction("Add Tab to Issue") {
                context.components.useCases.tabsUseCases.addTab
                    .invoke(issues + issueNumber)
            }
            .show()
    }

    companion object {
        const val issues = "https://github.com/mozilla-mobile/fenix/issues/"
        const val message = "Feature is not implemented, Issue #%"
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.app.Activity
import android.app.Dialog
import android.view.WindowManager

/**
 * Checks if activity's window has a FLAG_SECURE set and sets it to dialog
 */
fun Dialog.secure(activity: Activity?) {
    this.window.apply {
        val flags = activity?.window?.attributes?.flags
        if (flags != null && flags and WindowManager.LayoutParams.FLAG_SECURE != 0) {
            this?.setFlags(
                WindowManager.LayoutParams.FLAG_SECURE,
                WindowManager.LayoutParams.FLAG_SECURE,
            )
        }
    }
}

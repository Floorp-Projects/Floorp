/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.view.View
import android.widget.ImageButton

fun ImageButton.hideAndDisable() {
    this.visibility = View.INVISIBLE
    this.isEnabled = false
}

fun ImageButton.showAndEnable() {
    this.visibility = View.VISIBLE
    this.isEnabled = true
}

fun ImageButton.removeAndDisable() {
    this.visibility = View.GONE
    this.isEnabled = false
}

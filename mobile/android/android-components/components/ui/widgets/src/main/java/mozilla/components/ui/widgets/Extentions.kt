/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets

import android.widget.TextView
import android.view.View.TEXT_ALIGNMENT_CENTER as CENTER

/**
 * A shortcut to align buttons' text to center inside AlertDialog.
 */
fun androidx.appcompat.app.AlertDialog.withCenterAlignedButtons(): androidx.appcompat.app.AlertDialog {
    findViewById<TextView>(android.R.id.button1)?.let { it.textAlignment = CENTER }
    findViewById<TextView>(android.R.id.button2)?.let { it.textAlignment = CENTER }
    findViewById<TextView>(android.R.id.button3)?.let { it.textAlignment = CENTER }
    return this
}

/**
 * A shortcut to align buttons' text to center inside AlertDialog.
 *
 * Important: On Android API levels lower than 24, this method must be called only AFTER the dialog
 * has been shown. Calling this method prior to displaying the dialog on those API levels will cause
 * partial initialization of the view, leading to a crash.
 *
 * Usage example:
 * dialog.setOnShowListener {
 *     dialog.withCenterAlignedButtons()
 * }
 */
fun android.app.AlertDialog.withCenterAlignedButtons(): android.app.AlertDialog {
    findViewById<TextView>(android.R.id.button1)?.let { it.textAlignment = CENTER }
    findViewById<TextView>(android.R.id.button2)?.let { it.textAlignment = CENTER }
    findViewById<TextView>(android.R.id.button3)?.let { it.textAlignment = CENTER }
    return this
}

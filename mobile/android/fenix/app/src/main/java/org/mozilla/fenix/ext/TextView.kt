/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.text.style.UnderlineSpan
import android.widget.TextView
import androidx.core.text.toSpannable

/**
 * Adds an underline effect to the text displayed in the TextView.
 */
fun TextView.addUnderline() {
    val currentText = text
    text = currentText.toSpannable().apply {
        setSpan(UnderlineSpan(), 0, currentText.length, 0)
    }
}

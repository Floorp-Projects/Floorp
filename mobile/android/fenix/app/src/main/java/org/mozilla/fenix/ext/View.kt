/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.Rect
import android.view.TouchDelegate
import android.view.View

fun View?.increaseTapArea(extraDps: Int) {
    this!!.post {
        val touchRect = Rect()
        getHitRect(touchRect)
        touchRect.top -= extraDps
        touchRect.left -= extraDps
        touchRect.right += extraDps
        touchRect.bottom += extraDps
        (parent as View).touchDelegate = TouchDelegate(touchRect, this)
    }
}

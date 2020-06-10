/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.Rect
import android.view.TouchDelegate
import android.view.View
import mozilla.components.support.ktx.android.util.dpToPx

fun View.increaseTapArea(extraDps: Int) {
    val dips = extraDps.dpToPx(resources.displayMetrics)
    val parent = this.parent as View
    parent.post {
        val touchRect = Rect()
        getHitRect(touchRect)
        touchRect.inset(-dips, -dips)
        parent.touchDelegate = TouchDelegate(touchRect, this)
    }
}

fun View.removeTouchDelegate() {
    val parent = this.parent as View
    parent.post {
        parent.touchDelegate = null
    }
}

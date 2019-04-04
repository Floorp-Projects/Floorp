/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import android.util.TypedValue

fun Int.getColorFromAttr(context: Context): Int {
    val typedValue = TypedValue()
    val typedArray = context.obtainStyledAttributes(typedValue.data, intArrayOf(this))
    val color = typedArray.getColor(0, 0)
    typedArray.recycle()
    return color
}

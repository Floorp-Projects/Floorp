/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content.res

import android.content.res.Resources
import android.util.TypedValue

/**
 * Converts a value in density independent pixels (pxToDp) to the actual pixel values for the display.
 * @deprecated use Int.dpToPx instead.
 */
@Deprecated("Use Int.dpToPx instead", ReplaceWith(
    "pixels.dpToPx(this.displayMetrics)",
    "mozilla.components.support.ktx.android.util.Int.dpToPx"))
fun Resources.pxToDp(pixels: Int) = TypedValue.applyDimension(
        TypedValue.COMPLEX_UNIT_DIP, pixels.toFloat(), displayMetrics).toInt()

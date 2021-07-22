/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.widget

import android.view.View
import android.widget.TextView

/* This is the sum of both the default ascender height and the default descender height in Android */
private const val DEFAULT_FONT_PADDING = 6

/**
 * Adjusts the text size of the [TextView] according to the height restriction given to the
 * [View.MeasureSpec] given in the parameter.
 *
 * This will take [TextView.getIncludeFontPadding] into account when calculating the available height
 */
fun TextView.adjustMaxTextSize(heightMeasureSpec: Int, ascenderPadding: Int = DEFAULT_FONT_PADDING) {
    val maxHeight = View.MeasureSpec.getSize(heightMeasureSpec)

    var availableHeight = maxHeight.toFloat()
    if (this.includeFontPadding) {
        availableHeight -= ascenderPadding * resources.displayMetrics.scaledDensity
    }

    availableHeight -= (this.paddingBottom + this.paddingTop) *
        resources.displayMetrics.scaledDensity

    if (availableHeight > 0 && this.textSize > availableHeight) {
        this.textSize = availableHeight / resources.displayMetrics.scaledDensity
    }
}

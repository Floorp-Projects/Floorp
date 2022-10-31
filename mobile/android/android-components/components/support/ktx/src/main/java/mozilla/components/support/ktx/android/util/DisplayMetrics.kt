/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.util

import android.util.DisplayMetrics
import android.util.TypedValue

/**
 * Converts a value in density independent pixels (dp) to a float value.
 */
fun Int.dpToFloat(displayMetrics: DisplayMetrics) = TypedValue.applyDimension(
    TypedValue.COMPLEX_UNIT_DIP,
    this.toFloat(),
    displayMetrics,
)

/**
 * Converts a value in density independent pixels (dp) to the actual pixel values for the display.
 */
fun Int.dpToPx(displayMetrics: DisplayMetrics) = dpToFloat(displayMetrics).toInt()

/** Converts a value in density independent pixels (dp) to a px value. */
fun Float.dpToPx(displayMetrics: DisplayMetrics) = TypedValue.applyDimension(
    TypedValue.COMPLEX_UNIT_DIP,
    this,
    displayMetrics,
)

/** Converts a value in scale independent pixels (sp) to a px value. */
fun Float.spToPx(displayMetrics: DisplayMetrics) = TypedValue.applyDimension(
    TypedValue.COMPLEX_UNIT_SP,
    this,
    displayMetrics,
)

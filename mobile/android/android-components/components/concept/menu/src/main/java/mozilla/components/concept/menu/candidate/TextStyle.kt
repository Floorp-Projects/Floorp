/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu.candidate

import android.graphics.Typeface
import android.view.View
import androidx.annotation.ColorInt
import androidx.annotation.Dimension
import androidx.annotation.IntDef

/**
 * Describes styling for text inside a menu option.
 *
 * @param size: The size of the text.
 * @param color: The color to apply to the text.
 */
data class TextStyle(
    @Dimension(unit = Dimension.PX) val size: Float? = null,
    @ColorInt val color: Int? = null,
    @TypefaceStyle val textStyle: Int = Typeface.NORMAL,
    @TextAlignment val textAlignment: Int = View.TEXT_ALIGNMENT_INHERIT,
)

/**
 * Enum for [Typeface] values.
 */
@IntDef(value = [Typeface.NORMAL, Typeface.BOLD, Typeface.ITALIC, Typeface.BOLD_ITALIC])
annotation class TypefaceStyle

/**
 * Enum for text alignment values.
 */
@IntDef(
    value = [
        View.TEXT_ALIGNMENT_GRAVITY,
        View.TEXT_ALIGNMENT_INHERIT,
        View.TEXT_ALIGNMENT_CENTER,
        View.TEXT_ALIGNMENT_TEXT_START,
        View.TEXT_ALIGNMENT_TEXT_END,
        View.TEXT_ALIGNMENT_VIEW_START,
        View.TEXT_ALIGNMENT_VIEW_END,
    ],
)
annotation class TextAlignment

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.graphics.Color
import androidx.annotation.ColorInt
import androidx.core.graphics.ColorUtils

object ColorUtils {

    /**
     * Get text color (white or black) that is readable on top of the provided background color.
     */
    @JvmStatic
    fun getReadableTextColor(@ColorInt backgroundColor: Int): Int {
        return if (isDark(backgroundColor)) Color.WHITE else Color.BLACK
    }

    /**
     * @return true if the given [color] is dark enough that white text should be used on top of it.
     */
    @JvmStatic
    @SuppressWarnings("MagicNumber")
    fun isDark(@ColorInt color: Int): Boolean {
        if (color == Color.TRANSPARENT || ColorUtils.calculateLuminance(color) >= 0.5) {
            return false
        }

        val greyValue = grayscaleFromRGB(color)
        // 186 chosen rather than the seemingly obvious 128 because of gamma.
        return greyValue < 186
    }

    @SuppressWarnings("MagicNumber")
    private fun grayscaleFromRGB(@ColorInt color: Int): Int {
        val red = Color.red(color)
        val green = Color.green(color)
        val blue = Color.blue(color)
        // Magic weighting taken from a stackoverflow post, supposedly related to how
        // humans perceive color.
        return (0.299 * red + 0.587 * green + 0.114 * blue).toInt()
    }
}

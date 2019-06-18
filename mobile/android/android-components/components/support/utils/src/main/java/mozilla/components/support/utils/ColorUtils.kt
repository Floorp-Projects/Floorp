/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.graphics.Color

object ColorUtils {

    /**
     * Get text color (white or black) that is readable on top of the provided background color.
     */
    @JvmStatic
    @SuppressWarnings("MagicNumber")
    fun getReadableTextColor(backgroundColor: Int): Int {
        val greyValue = grayscaleFromRGB(backgroundColor)
        // 186 chosen rather than the seemingly obvious 128 because of gamma.
        return if (greyValue < 186) {
            Color.WHITE
        } else {
            Color.BLACK
        }
    }

    @SuppressWarnings("MagicNumber")
    private fun grayscaleFromRGB(color: Int): Int {
        val red = Color.red(color)
        val green = Color.green(color)
        val blue = Color.blue(color)
        // Magic weighting taken from a stackoverflow post, supposedly related to how
        // humans perceive color.
        return (0.299 * red + 0.587 * green + 0.114 * blue).toInt()
    }
}

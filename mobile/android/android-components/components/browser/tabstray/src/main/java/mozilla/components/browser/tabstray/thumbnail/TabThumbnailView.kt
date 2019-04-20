/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.browser.tabstray.thumbnail

import android.content.Context
import android.support.annotation.VisibleForTesting
import android.support.v7.widget.AppCompatImageView
import android.util.AttributeSet

class TabThumbnailView(context: Context, attrs: AttributeSet) : AppCompatImageView(context, attrs) {

    init {
        scaleType = ScaleType.MATRIX
    }

    @VisibleForTesting
    public override fun setFrame(l: Int, t: Int, r: Int, b: Int): Boolean {
        val changed = super.setFrame(l, t, r, b)
        if (changed) {
            val matrix = imageMatrix
            val scaleFactor = width / drawable.intrinsicWidth.toFloat()
            matrix.setScale(scaleFactor, scaleFactor, 0f, 0f)
            imageMatrix = matrix
        }
        return changed
    }
}

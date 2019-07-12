/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import android.util.AttributeSet
import androidx.appcompat.widget.AppCompatImageView

class ImageViewTopCrop(context: Context, attrs: AttributeSet) : AppCompatImageView(context, attrs) {
    override fun setFrame(l: Int, t: Int, r: Int, b: Int): Boolean {
        val scaleFactor = width / drawable.intrinsicWidth.toFloat()
        val matrix = imageMatrix

        matrix.setScale(scaleFactor, scaleFactor, 0f, 0f)
        imageMatrix = matrix

        return super.setFrame(l, t, r, b)
    }
}

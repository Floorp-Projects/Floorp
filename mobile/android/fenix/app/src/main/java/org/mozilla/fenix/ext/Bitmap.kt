/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.graphics.Bitmap
import android.graphics.Matrix
import android.view.View
import android.widget.ImageView

/**
 * This will scale the received [Bitmap] to the size of the [view]. It retains the bitmap's
 * original aspect ratio, but will shrink or enlarge it to fit the viewport. If bitmap does not
 * correctly fit the aspect ratio of the view, it will be shifted to prioritize the bottom-left
 * of the bitmap.
 */
fun Bitmap.scaleToBottomOfView(view: ImageView) {
    val bitmap = this
    view.setImageBitmap(bitmap)
    view.scaleType = ImageView.ScaleType.MATRIX
    val matrix = Matrix()
    view.addOnLayoutChangeListener(
        object : View.OnLayoutChangeListener {
            override fun onLayoutChange(
                v: View?,
                left: Int,
                top: Int,
                right: Int,
                bottom: Int,
                oldLeft: Int,
                oldTop: Int,
                oldRight: Int,
                oldBottom: Int,
            ) {
                val viewWidth: Float = view.width.toFloat()
                val viewHeight: Float = view.height.toFloat()
                val bitmapWidth = bitmap.width
                val bitmapHeight = bitmap.height
                val widthScale = viewWidth / bitmapWidth
                val heightScale = viewHeight / bitmapHeight
                val scale = widthScale.coerceAtLeast(heightScale)
                matrix.postScale(scale, scale)
                // The image is translated to its bottom such that any pertinent information is
                // guaranteed to be shown.
                // Majority of this math borrowed from // https://medium.com/@tokudu/how-to-whitelist-strictmode-violations-on-android-based-on-stacktrace-eb0018e909aa
                // except that there is no need to translate horizontally in our case.
                matrix.postTranslate(0f, (viewHeight - bitmapHeight * scale))
                view.imageMatrix = matrix
                view.removeOnLayoutChangeListener(this)
            }
        },
    )
}

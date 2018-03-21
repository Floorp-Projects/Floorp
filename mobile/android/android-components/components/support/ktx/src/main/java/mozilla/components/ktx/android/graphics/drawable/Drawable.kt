/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ktx.android.graphics.drawable

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable

/**
 * Converts a Drawable to Bitmap. via https://stackoverflow.com/a/46018816/2219998.
 */
@JvmOverloads
fun Drawable.toBitmap(bitmapConfig: Bitmap.Config = Bitmap.Config.ARGB_4444): Bitmap {
    if (this is BitmapDrawable) {
        return this.bitmap
    }

    val bitmap = Bitmap.createBitmap(intrinsicWidth, intrinsicHeight, bitmapConfig)
    val canvas = Canvas(bitmap)
    setBounds(0, 0, canvas.width, canvas.height)
    draw(canvas)
    return bitmap
}

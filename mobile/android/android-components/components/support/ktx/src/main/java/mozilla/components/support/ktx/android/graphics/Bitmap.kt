/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.graphics

import android.graphics.Bitmap
import android.graphics.BitmapShader
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Shader.TileMode
import android.util.Base64
import androidx.annotation.CheckResult
import java.io.ByteArrayOutputStream

/**
 * Transform bitmap into base64 encoded data uri (PNG).
 */
fun Bitmap.toDataUri(): String {
    val stream = ByteArrayOutputStream()
    compress(Bitmap.CompressFormat.PNG, BITMAP_COMPRESSION_QUALITY, stream)
    val encodedImage = Base64.encodeToString(stream.toByteArray(), Base64.DEFAULT)
    return "data:image/png;base64,$encodedImage"
}

private const val BITMAP_COMPRESSION_QUALITY = 100

/**
 * Returns a new bitmap that is the receiver Bitmap with four rounded corners;
 * the receiver is unmodified.
 *
 * This operation is expensive: it requires allocating an identical Bitmap and copying
 * all of the Bitmap's pixels. Consider these theoretically cheaper alternatives:
 * - android:background= a drawable with rounded corners
 * - Wrap your bitmap's ImageView with a layout that masks your view with rounded corners (e.g. CardView)
 */
@CheckResult
fun Bitmap.withRoundedCorners(cornerRadiusPx: Float): Bitmap {
    val roundedBitmap = Bitmap.createBitmap(width, height, config)
    val canvas = Canvas(roundedBitmap)
    val paint = Paint().apply {
        isAntiAlias = true
        shader = BitmapShader(this@withRoundedCorners, TileMode.CLAMP, TileMode.CLAMP)
    }

    canvas.drawRoundRect(
        0.0f,
        0.0f,
        width.toFloat(),
        height.toFloat(),
        cornerRadiusPx,
        cornerRadiusPx,
        paint,
    )
    return roundedBitmap
}

/**
 * Returns true if all pixels have the same value, false otherwise.
 */
fun Bitmap.arePixelsAllTheSame(): Boolean {
    val testPixel = getPixel(0, 0)

    // For perf, I expect iteration order is important. Under the hood, the pixels are represented
    // by a single array: if you iterate along the buffer, you can take advantage of cache hits
    // (since several words in memory are imported each time memory is accessed).
    //
    // We choose this iteration order (width first) because getPixels writes into a single array
    // with index 1 being the same value as getPixel(1, 0) (i.e. it writes width first).
    for (y in 0 until height) {
        for (x in 0 until width) {
            val color = getPixel(x, y)
            if (color != testPixel) {
                return false
            }
        }
    }

    return true
}

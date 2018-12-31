/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.graphics

import android.graphics.Bitmap
import android.graphics.BitmapShader
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Shader.TileMode
import android.support.annotation.CheckResult
import android.util.Base64
import java.io.ByteArrayOutputStream

/**
 * Transform bitmap into base64 encoded data uri (PNG).
 */
fun Bitmap.toDataUri(): String {
    val stream = ByteArrayOutputStream()
    compress(Bitmap.CompressFormat.PNG, 100, stream)
    val encodedImage = Base64.encodeToString(stream.toByteArray(), Base64.DEFAULT)
    return "data:image/png;base64," + encodedImage
}

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

    canvas.drawRoundRect(0.0f, 0.0f, width.toFloat(), height.toFloat(),
            cornerRadiusPx, cornerRadiusPx, paint)
    return roundedBitmap
}

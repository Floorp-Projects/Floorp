/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import android.graphics.Bitmap
import android.util.Size

/**
 * Scales a [Bitmap] to the given [size] while maintaining the aspect ratio.
 *
 * @param size The new [Size] to scale the [Bitmap] to.
 *
 * @return the scaled [Size].
 */
fun Bitmap.resizeMaintainingAspectRatio(size: Size) = if (width > height) {
    // Scale a wide bitmap
    val newMaxWidth = size.width
    val aspectRatio = height.toFloat() / width.toFloat()
    val scaledHeight = (newMaxWidth * aspectRatio).toInt()

    Size(newMaxWidth, scaledHeight)
} else {
    // Scale square or tall bitmap
    val newMaxHeight = size.height
    val aspectRatio = width.toFloat() / height.toFloat()
    val scaledWidth = (newMaxHeight * aspectRatio).toInt()

    Size(scaledWidth, newMaxHeight)
}

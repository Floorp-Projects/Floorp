/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.decoder

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import mozilla.components.browser.icons.DesiredSize
import mozilla.components.support.base.log.logger.Logger
import kotlin.math.max
import kotlin.math.min

/**
 * [IconDecoder] that will use Android's [BitmapFactory] in order to decode the byte data.
 */
class AndroidIconDecoder(
    private val ignoreSize: Boolean = false
) : IconDecoder {
    private val logger = Logger("AndroidIconDecoder")

    override fun decode(data: ByteArray, desiredSize: DesiredSize): Bitmap? =
        try {
            val (targetSize, maxSize, maxScaleFactor) = desiredSize
            val bitmap = decodeBitmap(data)

            when {
                bitmap == null -> null

                bitmap.width <= 0 || bitmap.height <= 0 -> {
                    logger.debug("BitmapFactory returned too small bitmap with width or height <= 0")
                    null
                }

                !ignoreSize && min(bitmap.width, bitmap.height) * maxScaleFactor < targetSize -> {
                    logger.debug("BitmapFactory returned too small bitmap")
                    null
                }

                !ignoreSize && max(bitmap.width, bitmap.height) * (1.0f / maxScaleFactor) > maxSize -> {
                    logger.debug("BitmapFactory returned way too large image")
                    null
                }

                else -> bitmap
            }
        } catch (e: OutOfMemoryError) {
            null
        }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun decodeBitmap(data: ByteArray): Bitmap? {
        return BitmapFactory.decodeByteArray(data, 0, data.size)
    }
}

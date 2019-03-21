/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.decoder

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.support.annotation.VisibleForTesting
import android.support.annotation.VisibleForTesting.PRIVATE
import mozilla.components.support.base.log.logger.Logger

/**
 * [IconDecoder] that will use Android's [BitmapFactory] in order to decode the byte data.
 */
class AndroidIconDecoder : IconDecoder {
    private val logger = Logger("AndroidIconDecoder")

    override fun decode(data: ByteArray, targetSize: Int, maxSize: Int, maxScaleFactor: Float): Bitmap? {
        try {
            val bitmap = decodeBitmap(data)

            return when {
                bitmap == null -> null

                bitmap.width <= 0 || bitmap.height <= 0 -> {
                    logger.debug("BitmapFactory returned too small bitmap with width or height <= 0")
                    null
                }

                bitmap.width * maxScaleFactor < targetSize || bitmap.height * maxScaleFactor < targetSize -> {
                    logger.debug("BitmapFactory returned too small bitmap")
                    null
                }

                bitmap.width * (1.0f / maxScaleFactor) > maxSize ||
                    bitmap.height * (1.0f / maxScaleFactor) > maxSize -> {
                    logger.debug("BitmapFactory returned way too large image")
                    null
                }

                else -> bitmap
            }
        } catch (e: OutOfMemoryError) {
            return null
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun decodeBitmap(data: ByteArray): Bitmap? {
        return BitmapFactory.decodeByteArray(data, 0, data.size)
    }
}

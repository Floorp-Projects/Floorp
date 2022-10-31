/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images.decoder

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.Size
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.images.DesiredSize
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

/**
 * [ImageDecoder] that will use Android's [BitmapFactory] in order to decode the byte data.
 */
class AndroidImageDecoder : ImageDecoder {
    private val logger = Logger("AndroidImageDecoder")

    override fun decode(data: ByteArray, desiredSize: DesiredSize): Bitmap? =
        try {
            val bounds = decodeBitmapBounds(data)
            val maxBoundLength = max(bounds.width, bounds.height).toFloat()

            val sampleSize = floor(maxBoundLength / desiredSize.targetSize.toFloat()).toInt()

            if (isGoodSize(bounds, desiredSize)) {
                decodeBitmap(data, sampleSize)
            } else {
                null
            }
        } catch (e: OutOfMemoryError) {
            null
        }

    private fun isGoodSize(bounds: Size, desiredSize: DesiredSize): Boolean {
        val (_, minSize, maxSize, maxScaleFactor) = desiredSize
        return when {
            min(bounds.width, bounds.height) <= 0 -> {
                logger.debug("BitmapFactory returned too small bitmap with width or height <= 0")
                false
            }

            min(bounds.width, bounds.height) * maxScaleFactor < minSize -> {
                logger.debug("BitmapFactory returned too small bitmap")
                false
            }

            max(bounds.width, bounds.height) * (1f / maxScaleFactor) > maxSize -> {
                logger.debug("BitmapFactory returned way too large image")
                false
            }

            else -> true
        }
    }

    /**
     * Decodes the width and height of a bitmap without loading it into memory.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun decodeBitmapBounds(data: ByteArray): Size {
        val options = BitmapFactory.Options().apply {
            inJustDecodeBounds = true
        }
        BitmapFactory.decodeByteArray(data, 0, data.size, options)
        return Size(options.outWidth, options.outHeight)
    }

    /**
     * Decodes a bitmap image.
     *
     * @param data Image bytes to decode.
     * @param sampleSize Scale factor for the image.
     */
    @VisibleForTesting(otherwise = PRIVATE)
    internal fun decodeBitmap(data: ByteArray, sampleSize: Int): Bitmap? {
        val options = BitmapFactory.Options().apply {
            inSampleSize = sampleSize
        }
        return BitmapFactory.decodeByteArray(data, 0, data.size, options)
    }
}

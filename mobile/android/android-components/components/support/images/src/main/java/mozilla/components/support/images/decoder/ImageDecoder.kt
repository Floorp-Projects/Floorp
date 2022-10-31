/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images.decoder

import android.graphics.Bitmap
import mozilla.components.support.images.DesiredSize

/**
 * An image decoder that can decode a [ByteArray] into a [Bitmap].
 *
 * Depending on the image format the decoder may internally decode the [ByteArray] into multiple [Bitmap]. It is up to
 * the decoder implementation to return the best [Bitmap] to use.
 */
interface ImageDecoder {
    /**
     * Decodes the given [data] into a a [Bitmap] or null.
     *
     * The caller provides a maximum size. This is useful for image formats that may decode into multiple images. The
     * decoder can use this size to determine which [Bitmap] to return.
     */
    fun decode(data: ByteArray, desiredSize: DesiredSize): Bitmap?

    companion object {
        @Suppress("MagicNumber")
        enum class ImageMagicNumbers(var value: ByteArray) {
            // It is irritating that Java bytes are signed...
            PNG(byteArrayOf((0x89 and 0xFF).toByte(), 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a)),
            GIF(byteArrayOf(0x47, 0x49, 0x46, 0x38)),
            JPEG(byteArrayOf(-0x1, -0x28, -0x1, -0x20)),
            BMP(byteArrayOf(0x42, 0x4d)),
            WEB(byteArrayOf(0x57, 0x45, 0x42, 0x50, 0x0a)),
        }
    }
}

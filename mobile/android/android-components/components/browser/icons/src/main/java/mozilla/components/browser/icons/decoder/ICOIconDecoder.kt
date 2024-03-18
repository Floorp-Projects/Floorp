/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.decoder

import android.graphics.Bitmap
import mozilla.components.browser.icons.decoder.ico.decodeDirectoryEntries
import mozilla.components.browser.icons.utils.findBestSize
import mozilla.components.support.images.DesiredSize
import mozilla.components.support.images.decoder.ImageDecoder

// Some geometry of an ICO file.
internal const val HEADER_LENGTH_BYTES = 6
internal const val ICON_DIRECTORY_ENTRY_LENGTH_BYTES = 16

internal const val ZERO_BYTE = 0.toByte()

/**
 * [ImageDecoder] implementation for decoding ICO files.
 *
 * An ICO file is a container format that may hold up to 255 images in either BMP or PNG format.
 * A mixture of image types may not exist.
 */
class ICOIconDecoder : ImageDecoder {
    override fun decode(data: ByteArray, desiredSize: DesiredSize): Bitmap? {
        val (targetSize, _, maxSize, maxScaleFactor) = desiredSize
        val entries = decodeDirectoryEntries(data, maxSize)

        val bestEntry = entries.map { entry ->
            Pair(entry.width, entry.height)
        }.findBestSize(targetSize, maxSize, maxScaleFactor) ?: return null

        for (entry in entries) {
            if (entry.width == bestEntry.first && entry.height == bestEntry.second) {
                return entry.toBitmap(data)
            }
        }

        return null
    }
}

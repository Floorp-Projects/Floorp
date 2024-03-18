/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.decoder.ico

import android.graphics.Bitmap
import mozilla.components.browser.icons.decoder.HEADER_LENGTH_BYTES
import mozilla.components.browser.icons.decoder.ICON_DIRECTORY_ENTRY_LENGTH_BYTES
import mozilla.components.browser.icons.decoder.ZERO_BYTE
import mozilla.components.support.images.decoder.ImageDecoder
import mozilla.components.support.ktx.kotlin.containsAtOffset
import mozilla.components.support.ktx.kotlin.toBitmap

const val MAX_BITS_PER_PIXEL = 32

internal data class IconDirectoryEntry(
    val width: Int,
    val height: Int,
    val paletteSize: Int,
    val bitsPerPixel: Int,
    val payloadSize: Int,
    val payloadOffset: Int,
    val payloadIsPNG: Boolean,
    val directoryIndex: Int,
) : Comparable<IconDirectoryEntry> {

    override fun compareTo(other: IconDirectoryEntry): Int = when {
        width > other.width -> 1
        width < other.width -> -1

        // Where both images exceed the max BPP, take the smaller of the two BPP values.
        bitsPerPixel >= MAX_BITS_PER_PIXEL && other.bitsPerPixel >= MAX_BITS_PER_PIXEL &&
            bitsPerPixel < other.bitsPerPixel -> 1
        bitsPerPixel >= MAX_BITS_PER_PIXEL && other.bitsPerPixel >= MAX_BITS_PER_PIXEL &&
            bitsPerPixel > other.bitsPerPixel -> -1

        // Otherwise, take the larger of the BPP values.
        bitsPerPixel > other.bitsPerPixel -> 1
        bitsPerPixel < other.bitsPerPixel -> -1

        // Prefer large palettes.
        paletteSize > other.paletteSize -> 1
        paletteSize < other.paletteSize -> -1

        // Prefer smaller payloads.
        payloadSize < other.payloadSize -> 1
        payloadSize > other.payloadSize -> -1

        // If all else fails, prefer PNGs over BMPs. They tend to be smaller.
        payloadIsPNG && !other.payloadIsPNG -> 1
        !payloadIsPNG && other.payloadIsPNG -> -1

        else -> 0
    }

    @Suppress("MagicNumber")
    fun toBitmap(data: ByteArray): Bitmap? {
        if (payloadIsPNG) {
            // PNG payload. Simply extract it and let Android decode it.
            return data.toBitmap(payloadOffset, payloadSize)
        }

        // The payload is a BMP, so we need to do some magic to get the decoder to do what we want.
        // We construct an ICO containing just the image we want, and let Android do the rest.
        val decodeTarget = ByteArray(HEADER_LENGTH_BYTES + ICON_DIRECTORY_ENTRY_LENGTH_BYTES + payloadSize)

        // Set the type field in the ICO header.
        decodeTarget[2] = 1.toByte()

        // Set the num-images field in the header to 1.
        decodeTarget[4] = 1.toByte()

        // Copy the ICONDIRENTRY we need into the new buffer.
        val offset = HEADER_LENGTH_BYTES + (directoryIndex * ICON_DIRECTORY_ENTRY_LENGTH_BYTES)
        System.arraycopy(data, offset, decodeTarget, HEADER_LENGTH_BYTES, ICON_DIRECTORY_ENTRY_LENGTH_BYTES)

        val singlePayloadOffset = HEADER_LENGTH_BYTES + ICON_DIRECTORY_ENTRY_LENGTH_BYTES

        System.arraycopy(data, payloadOffset, decodeTarget, singlePayloadOffset, payloadSize)

        // Update the offset field of the ICONDIRENTRY to make the new ICO valid.
        decodeTarget[HEADER_LENGTH_BYTES + 12] = singlePayloadOffset.toByte()
        decodeTarget[HEADER_LENGTH_BYTES + 13] = singlePayloadOffset.ushr(8).toByte()
        decodeTarget[HEADER_LENGTH_BYTES + 14] = singlePayloadOffset.ushr(16).toByte()
        decodeTarget[HEADER_LENGTH_BYTES + 15] = singlePayloadOffset.ushr(24).toByte()

        return decodeTarget.toBitmap()
    }
}

/**
 * The format consists of a header specifying the number, n, of images, followed by the Icon Directory.
 *
 * The Icon Directory consists of n Icon Directory Entries, each 16 bytes in length, specifying, for
 * the corresponding image, the dimensions, colour information, payload size, and location in the file.
 *
 * All numerical fields follow a little-endian byte ordering.
 *
 * Header format:
 *
 * ```
 *  0               1               2               3
 *  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Reserved field. Must be zero |  Type (1 for ICO, 2 for CUR)  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Image count (n)       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * ```
 *
 * The type field is expected to always be 1. CUR format images should not be used for Favicons.
 *
 *
 * Icon Directory Entry format:
 * ```
 *  0               1               2               3
 *  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Image width  | Image height  | Palette size  | Reserved (0)  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |       Colour plane count      |         Bits per pixel        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                   Size of image data, in bytes                |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      Start of image data, as an offset from start of file     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * ```
 *
 * Image dimensions of zero are to be interpreted as image dimensions of 256.
 *
 * The palette size field records the number of colours in the stored BMP, if a palette is used. Zero
 * if the payload is a PNG or no palette is in use.
 *
 * The number of colour planes is, usually, 0 (Not in use) or 1. Values greater than 1 are to be
 * interpreted not as a colour plane count, but as a multiplying factor on the bits per pixel field.
 * (Apparently 65535 was not deemed a sufficiently large maximum value of bits per pixel.)
 *
 * The Icon Directory consists of n-many Icon Directory Entries in sequence, with no gaps.
 */
@Suppress("MagicNumber", "ReturnCount", "ComplexMethod", "NestedBlockDepth", "ComplexCondition")
internal fun decodeDirectoryEntries(data: ByteArray, maxSize: Int): List<IconDirectoryEntry> {
    // Fail if we don't have enough space for the header.
    if (data.size < HEADER_LENGTH_BYTES) {
        return emptyList()
    }

    // Check that the reserved fields in the header are indeed zero, and that the type field
    // specifies ICO. If not, we've probably been given something that isn't really an ICO.
    if (data[0] != ZERO_BYTE ||
        data[1] != ZERO_BYTE ||
        data[2] != 1.toByte() ||
        data[3] != ZERO_BYTE
    ) {
        return emptyList()
    }

    // Here, and in many other places, byte values are ANDed with 0xFF. This is because Java
    // bytes are signed - to obtain a numerical value of a longer type which holds the unsigned
    // interpretation of the byte of interest, we do this.
    val numEncodedImages = (data[4].toInt() and 0xFF) or ((data[5].toInt() and 0xFF) shl 8)

    // Fail if there are no images or the field is corrupt.
    if (numEncodedImages <= 0) {
        return emptyList()
    }

    val headerAndDirectorySize = HEADER_LENGTH_BYTES + numEncodedImages * ICON_DIRECTORY_ENTRY_LENGTH_BYTES

    // Fail if there is not enough space in the buffer for the stated number of icondir entries,
    // let alone the data.
    if (data.size < headerAndDirectorySize) {
        return emptyList()
    }

    // Put the pointer on the first byte of the first Icon Directory Entry.
    var bufferIndex = HEADER_LENGTH_BYTES

    // We now iterate over the Icon Directory, decoding each entry as we go. We also need to
    // discard all entries except one >= the maximum interesting size.

    // Size of the smallest image larger than the limit encountered.
    var minimumMaximum = Integer.MAX_VALUE

    // Used to track the best entry for each size. The entries we want to keep.
    val iconMap = mutableMapOf<Int, IconDirectoryEntry>()

    var i = 0
    while (i < numEncodedImages) {
        // Decode the Icon Directory Entry at this offset.
        val newEntry = createIconDirectoryEntry(data, bufferIndex, i)

        if (newEntry == null) {
            i++
            bufferIndex += ICON_DIRECTORY_ENTRY_LENGTH_BYTES
            continue
        }

        if (newEntry.width > maxSize) {
            // If we already have a smaller image larger than the maximum size of interest, we
            // don't care about the new one which is larger than the smallest image larger than
            // the maximum size.
            if (newEntry.width >= minimumMaximum) {
                i++
                bufferIndex += ICON_DIRECTORY_ENTRY_LENGTH_BYTES
                continue
            }

            // Remove the previous minimum-maximum.
            iconMap.remove(minimumMaximum)

            minimumMaximum = newEntry.width
        }

        val oldEntry = iconMap[newEntry.width]
        if (oldEntry == null) {
            iconMap[newEntry.width] = newEntry
            i++
            bufferIndex += ICON_DIRECTORY_ENTRY_LENGTH_BYTES
            continue
        }

        if (oldEntry < newEntry) {
            iconMap[newEntry.width] = newEntry
        }
        i++
        bufferIndex += ICON_DIRECTORY_ENTRY_LENGTH_BYTES
    }

    val count = iconMap.size

    // Abort if no entries are desired (Perhaps all are corrupt?)
    if (count == 0) {
        return emptyList()
    }

    return iconMap.values.toList()
}

@Suppress("MagicNumber")
internal fun createIconDirectoryEntry(
    data: ByteArray,
    entryOffset: Int,
    directoryIndex: Int,
): IconDirectoryEntry? {
    // Verify that the reserved field is really zero.
    if (data[entryOffset + 3] != ZERO_BYTE) {
        return null
    }

    // Verify that the entry points to a region that actually exists in the buffer, else bin it.
    var fieldPtr = entryOffset + 8
    val entryLength = data[fieldPtr].toInt() and 0xFF or (
        (data[fieldPtr + 1].toInt() and 0xFF) shl 8
        ) or (
        (data[fieldPtr + 2].toInt() and 0xFF) shl 16
        ) or (
        (data[fieldPtr + 3].toInt() and 0xFF) shl 24
        )

    // Advance to the offset field.
    fieldPtr += 4

    val payloadOffset = data[fieldPtr].toInt() and 0xFF or (
        (data[fieldPtr + 1].toInt() and 0xFF) shl 8
        ) or (
        (data[fieldPtr + 2].toInt() and 0xFF) shl 16
        ) or (
        (data[fieldPtr + 3].toInt() and 0xFF) shl 24
        )

    // Fail if the entry describes a region outside the buffer.
    if (payloadOffset < 0 || entryLength < 0 || payloadOffset + entryLength > data.size) {
        return null
    }

    // Extract the image dimensions.
    var imageWidth = data[entryOffset].toInt() and 0xFF
    var imageHeight = data[entryOffset + 1].toInt() and 0xFF

    // Because Microsoft, a size value of zero represents an image size of 256.
    if (imageWidth == 0) {
        imageWidth = 256
    }

    if (imageHeight == 0) {
        imageHeight = 256
    }

    // If the image uses a colour palette, this is the number of colours, otherwise this is zero.
    val paletteSize = data[entryOffset + 2].toInt() and 0xFF

    // The plane count - usually 0 or 1. When > 1, taken as multiplier on bitsPerPixel.
    val colorPlanes = data[entryOffset + 4].toInt() and 0xFF

    var bitsPerPixel = (data[entryOffset + 6].toInt() and 0xFF) or ((data[entryOffset + 7].toInt() and 0xFF) shl 8)

    if (colorPlanes > 1) {
        bitsPerPixel *= colorPlanes
    }

    // Look for PNG magic numbers at the start of the payload.
    val payloadIsPNG = data.containsAtOffset(payloadOffset, ImageDecoder.Companion.ImageMagicNumbers.PNG.value)

    return IconDirectoryEntry(
        imageWidth,
        imageHeight,
        paletteSize,
        bitsPerPixel,
        entryLength,
        payloadOffset,
        payloadIsPNG,
        directoryIndex,
    )
}

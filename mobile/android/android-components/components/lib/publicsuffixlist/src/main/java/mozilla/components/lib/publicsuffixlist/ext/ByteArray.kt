/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.publicsuffixlist.ext

import kotlin.experimental.and

private const val BITMASK = 0xff.toByte()

/**
 * Performs a binary search for the provided [labels] on the [ByteArray]'s data.
 *
 * This algorithm is based on OkHttp's PublicSuffixDatabase class:
 * https://github.com/square/okhttp/blob/master/okhttp/src/main/java/okhttp3/internal/publicsuffix/PublicSuffixDatabase.java
 */
@Suppress("ComplexMethod", "NestedBlockDepth")
internal fun ByteArray.binarySearch(labels: List<ByteArray>, labelIndex: Int): String? {
    var low = 0
    var high = size
    var match: String? = null

    while (low < high) {
        val mid = (low + high) / 2
        val start = findStartOfLineFromIndex(mid)
        val end = findEndOfLineFromIndex(start)

        val publicSuffixLength = start + end - start

        var compareResult: Int
        var currentLabelIndex = labelIndex
        var currentLabelByteIndex = 0
        var publicSuffixByteIndex = 0

        var expectDot = false
        while (true) {
            val byte0 = if (expectDot) {
                expectDot = false
                '.'.code.toByte()
            } else {
                labels[currentLabelIndex][currentLabelByteIndex] and BITMASK
            }

            val byte1 = this[start + publicSuffixByteIndex] and BITMASK

            // Compare the bytes. Note that the file stores UTF-8 encoded bytes, so we must compare the
            // unsigned bytes.
            @Suppress("EXPERIMENTAL_API_USAGE")
            compareResult = (byte0.toUByte() - byte1.toUByte()).toInt()
            if (compareResult != 0) {
                break
            }

            publicSuffixByteIndex++
            currentLabelByteIndex++

            if (publicSuffixByteIndex == publicSuffixLength) {
                break
            }

            if (labels[currentLabelIndex].size == currentLabelByteIndex) {
                // We've exhausted our current label. Either there are more labels to compare, in which
                // case we expect a dot as the next character. Otherwise, we've checked all our labels.
                if (currentLabelIndex == labels.size - 1) {
                    break
                } else {
                    currentLabelIndex++
                    currentLabelByteIndex = -1
                    expectDot = true
                }
            }
        }

        if (compareResult < 0) {
            high = start - 1
        } else if (compareResult > 0) {
            low = start + end + 1
        } else {
            // We found a match, but are the lengths equal?
            val publicSuffixBytesLeft = publicSuffixLength - publicSuffixByteIndex
            var labelBytesLeft = labels[currentLabelIndex].size - currentLabelByteIndex
            for (i in currentLabelIndex + 1 until labels.size) {
                labelBytesLeft += labels[i].size
            }

            if (labelBytesLeft < publicSuffixBytesLeft) {
                high = start - 1
            } else if (labelBytesLeft > publicSuffixBytesLeft) {
                low = start + end + 1
            } else {
                // Found a match.
                match = String(this, start, publicSuffixLength, Charsets.UTF_8)
                break
            }
        }
    }

    return match
}

/**
 * Search for a '\n' that marks the start of a value. Don't go back past the start of the array.
 */
private fun ByteArray.findStartOfLineFromIndex(start: Int): Int {
    var index = start
    while (index > -1 && this[index] != '\n'.code.toByte()) {
        index--
    }
    index++
    return index
}

/**
 * Search for a '\n' that marks the end of a value.
 */
private fun ByteArray.findEndOfLineFromIndex(start: Int): Int {
    var end = 1
    while (this[start + end] != '\n'.code.toByte()) {
        end++
    }
    return end
}

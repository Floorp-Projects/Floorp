/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.utils

/**
 * For a list of size [Pair]s (width, height) returns the best [Pair] for the given [targetSize], [maxSize] and
 * [maxScaleFactor].
 */
@Suppress("ReturnCount")
internal fun List<Pair<Int, Int>>.findBestSize(targetSize: Int, maxSize: Int, maxScaleFactor: Float): Pair<Int, Int>? {
    // First look for a pair that is close to (but not smaller than) our target size.
    val ideal = filter { (x, y) ->
        x >= targetSize && y >= targetSize && x <= maxSize && y <= maxSize
    }.filter { (x, y) ->
        x == y
    }.minByOrNull { (x, _) ->
        x
    }

    if (ideal != null) {
        // We found an icon that is exactly in the range we want, yay!
        return ideal
    }

    // Next try to find a pair that is larger than our target size but that we can downscale to be in our target range.
    val downscalable = filter { (x, y) ->
        val downScaledX = (x.toFloat() * (1.0f / maxScaleFactor)).toInt()
        val downScaledY = (y.toFloat() * (1.0f / maxScaleFactor)).toInt()

        downScaledX >= targetSize &&
            downScaledY >= targetSize &&
            downScaledX <= maxSize &&
            downScaledY <= maxSize
    }.minByOrNull { (x, _) ->
        x
    }

    if (downscalable != null) {
        // We found an icon we can downscale to our desired size.
        return downscalable
    }

    // Finally try to find a pair that is smaller than our target size but that we can upscale to our target range.
    val upscalable = filter { (x, y) ->
        val upscaledX = x * maxScaleFactor
        val upscaledY = y * maxScaleFactor

        upscaledX >= targetSize &&
            upscaledY >= targetSize &&
            upscaledX <= maxSize &&
            upscaledY <= maxSize
    }.maxByOrNull { (x, _) ->
        x
    }

    if (upscalable != null) {
        // We found an icon we can upscale to our desired size.
        return upscalable
    }

    return null
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.content.Context
import android.graphics.Bitmap
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.images.DesiredSize
import kotlin.math.roundToInt

/**
 * [IconProcessor] implementation for resizing the loaded icon based on the target size.
 */
class ResizingProcessor(
    private val discardSmallIcons: Boolean = true,
) : IconProcessor {

    override fun process(
        context: Context,
        request: IconRequest,
        resource: IconRequest.Resource?,
        icon: Icon,
        desiredSize: DesiredSize,
    ): Icon? {
        val originalBitmap = icon.bitmap
        val size = originalBitmap.width
        val (targetSize, _, _, maxScaleFactor) = desiredSize

        // The bitmap has exactly the size we are looking for.
        if (size == targetSize) return icon

        val resizedBitmap = if (size > targetSize) {
            resize(originalBitmap, targetSize)
        } else {
            // Our largest primary is smaller than the desired size. Upscale it (to a limit)!
            // 'largestSize' now reflects the maximum size we can upscale to.
            val largestSize = (size * maxScaleFactor).roundToInt()

            if (largestSize > targetSize) {
                // Perfect! WE can upscale and reach the needed size.
                resize(originalBitmap, targetSize)
            } else {
                // We don't have enough information to make the target size look non-terrible.
                // Best effort scale, unless we're told to throw away small icons.
                if (discardSmallIcons) null else resize(originalBitmap, largestSize)
            }
        }

        return icon.copy(bitmap = resizedBitmap ?: return null)
    }

    /**
     * Create a new bitmap scaled from an existing bitmap.
     */
    @VisibleForTesting
    internal fun resize(bitmap: Bitmap, targetSize: Int) = try {
        Bitmap.createScaledBitmap(bitmap, targetSize, targetSize, true)
    } catch (e: OutOfMemoryError) {
        // There's not enough memory to create a resized copy of the bitmap in memory. Let's just
        // use what we have.
        bitmap
    }
}

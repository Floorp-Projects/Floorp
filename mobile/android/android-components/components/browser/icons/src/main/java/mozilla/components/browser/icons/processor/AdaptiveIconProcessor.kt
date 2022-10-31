/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.content.Context
import android.graphics.Paint
import android.graphics.Paint.ANTI_ALIAS_FLAG
import android.graphics.Rect
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.core.graphics.applyCanvas
import androidx.core.graphics.createBitmap
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.images.DesiredSize
import kotlin.math.max

/**
 * [IconProcessor] implementation that builds maskable icons.
 */
class AdaptiveIconProcessor : IconProcessor {

    /**
     * Creates an adaptive icon using the base icon.
     * On older devices, non-maskable icons are not transformed.
     */
    override fun process(
        context: Context,
        request: IconRequest,
        resource: IconRequest.Resource?,
        icon: Icon,
        desiredSize: DesiredSize,
    ): Icon {
        val maskable = resource?.maskable == true
        if (!maskable && SDK_INT < Build.VERSION_CODES.O) {
            return icon
        }

        val originalBitmap = icon.bitmap

        val paddingRatio = if (maskable) {
            MASKABLE_ICON_PADDING_RATIO
        } else {
            TRANSPARENT_ICON_PADDING_RATIO
        }
        val maskedIconSize = max(originalBitmap.width, originalBitmap.height)
        val padding = (paddingRatio * maskedIconSize).toInt()

        // The actual size of the icon asset, before masking, in pixels.
        val rawIconSize = 2 * padding + maskedIconSize
        val maskBounds = Rect(0, 0, maskedIconSize, maskedIconSize).apply {
            offset(padding, padding)
        }

        val paint = Paint(ANTI_ALIAS_FLAG).apply { isFilterBitmap = true }

        val paddedBitmap = createBitmap(rawIconSize, rawIconSize).applyCanvas {
            icon.color?.also { drawColor(it) }
            drawBitmap(originalBitmap, null, maskBounds, paint)
        }

        return icon.copy(bitmap = paddedBitmap, maskable = true).also { originalBitmap.recycle() }
    }

    companion object {
        private const val MASKABLE_ICON_SAFE_ZONE = 4 / 5f
        private const val ADAPTIVE_ICON_SAFE_ZONE = 66 / 108f
        private const val TRANSPARENT_ICON_SAFE_ZONE = 192 / 176f
        private const val MASKABLE_ICON_PADDING_RATIO =
            ((MASKABLE_ICON_SAFE_ZONE / ADAPTIVE_ICON_SAFE_ZONE) - 1) / 2
        private const val TRANSPARENT_ICON_PADDING_RATIO =
            ((TRANSPARENT_ICON_SAFE_ZONE / ADAPTIVE_ICON_SAFE_ZONE) - 1) / 2
    }
}

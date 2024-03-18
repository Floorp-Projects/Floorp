/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.content.Context
import android.graphics.Color
import androidx.palette.graphics.Palette
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.images.DesiredSize

/**
 * [IconProcessor] implementation to extract the dominant color from the icon.
 */
class ColorProcessor : IconProcessor {

    override fun process(
        context: Context,
        request: IconRequest,
        resource: IconRequest.Resource?,
        icon: Icon,
        desiredSize: DesiredSize,
    ): Icon {
        // If the icon already has a color set, just return
        if (icon.color != null) return icon
        // If the request already has a color set, then return.
        // Some PWAs just set the background color to white (such as Twitter, Starbucks)
        // but the icons would work better if we fill the background using the Palette API.
        // If a PWA really want a white background a maskable icon can be used.
        if (request.color != null && request.color != Color.WHITE) return icon.copy(color = request.color)

        val swatch = Palette.from(icon.bitmap).generate().dominantSwatch
        return swatch?.run { icon.copy(color = rgb) } ?: icon
    }
}

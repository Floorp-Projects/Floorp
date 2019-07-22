/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.content.Context
import androidx.palette.graphics.Palette
import mozilla.components.browser.icons.DesiredSize
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest

/**
 * [IconProcessor] implementation to extract the dominant color from the icon.
 */
class ColorProcessor : IconProcessor {

    override fun process(
        context: Context,
        request: IconRequest,
        resource: IconRequest.Resource?,
        icon: Icon,
        desiredSize: DesiredSize
    ): Icon {
        if (icon.color != null) return icon
        if (request.color != null) return icon.copy(color = request.color)

        val swatch = Palette.from(icon.bitmap).generate().dominantSwatch
        return swatch?.run { icon.copy(color = rgb) } ?: icon
    }
}

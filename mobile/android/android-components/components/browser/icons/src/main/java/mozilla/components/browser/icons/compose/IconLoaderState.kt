/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.compose

import androidx.compose.ui.graphics.painter.Painter
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.Icon.Source

/**
 * The state an [IconLoaderScope] is in.
 */
sealed class IconLoaderState {
    /**
     * The [BrowserIcons.Loader] is currently loading the icon.
     */
    object Loading : IconLoaderState()

    /**
     * The [BrowserIcons.Loader] has completed loading the icon and it is available through the
     * attached [painter].
     *
     * @property painter The loaded or generated icon as a [Painter].
     * @property color The dominant color of the icon. Will be null if no color could be extracted.
     * @property source The source of the icon.
     * @property maskable True if the icon represents as full-bleed icon that can be cropped to other shapes.
     */
    data class Icon(
        val painter: Painter,
        val color: Int?,
        val source: Source,
        val maskable: Boolean,
    ) : IconLoaderState()
}

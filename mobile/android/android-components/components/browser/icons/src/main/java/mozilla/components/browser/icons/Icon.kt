/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

import android.graphics.Bitmap

/**
 * An [Icon] returned by [BrowserIcons] after processing an [IconRequest]
 *
 * @property bitmap The loaded icon as a [Bitmap] or null if no icon could be loaded.
 * @property color The dominant color of the icon. Will be null if no color could be extracted.
 * @property source The source of the icon.
 */
data class Icon(
    val bitmap: Bitmap? = null,
    val color: Int? = null,
    val source: Source
) {
    /**
     * The source of an [Icon].
     */
    enum class Source {
        /**
         * This icon was generated.
         */
        GENERATOR,

        /**
         * This icon was downloaded.
         */
        DOWNLOAD
    }
}

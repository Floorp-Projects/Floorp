/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

import android.graphics.Bitmap

/**
 * An [Icon] returned by [BrowserIcons] after processing an [IconRequest]
 *
 * @property bitmap The loaded icon as a [Bitmap].
 * @property color The dominant color of the icon. Will be null if no color could be extracted.
 * @property source The source of the icon.
 * @property maskable True if the icon represents as full-bleed icon that can be cropped to other shapes.
 */
data class Icon(
    val bitmap: Bitmap,
    val color: Int? = null,
    val source: Source,
    val maskable: Boolean = false,
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
        DOWNLOAD,

        /**
         * This icon was inlined in the document.
         */
        INLINE,

        /**
         * This icon was loaded from an in-memory cache.
         */
        MEMORY,

        /**
         * This icon was loaded from a disk cache.
         */
        DISK,
    }
}

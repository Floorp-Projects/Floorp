/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

/**
 * A request to load an [Icon].
 *
 * @property url The URL of the website an icon should be loaded for.
 * @property size The preferred size of the icon that should be loaded.
 * @property resources An optional list of icon resources to load the icon from.
 */
data class IconRequest(
    val url: String,
    val size: Size = Size.DEFAULT,
    val resources: List<Resource> = emptyList()
) {
    /**
     * Supported sizes.
     *
     * We are trying to limit the supported sizes in order to optimize our caching strategy.
     */
    @Suppress("MagicNumber")
    enum class Size(
        val value: Int
    ) {
        DEFAULT(32),
        LAUNCHER(48)
    }

    /**
     * An icon resource that can be loaded.
     *
     * @param url URL the icon resource can be fetched from.
     * @param type The type of the icon.
     * @param sizes Optional list of icon sizes provided by this resource (if known).
     * @param mimeType Optional MIME type of this icon resource (if known).
     */
    data class Resource(
        val url: String,
        val type: Type,
        val sizes: List<Size> = emptyList(),
        val mimeType: String? = null
    ) {
        /**
         * An icon resource type.
         */
        enum class Type {
            /**
             * A favicon ("icon" or "shortcut icon").
             *
             * https://en.wikipedia.org/wiki/Favicon
             */
            FAVICON,

            /**
             * An Apple touch icon.
             *
             * Originally used for adding an icon to the home screen of an iOS device.
             *
             * https://realfavicongenerator.net/blog/apple-touch-icon-the-good-the-bad-the-ugly/
             */
            APPLE_TOUCH_ICON,

            /**
             * A "fluid" icon.
             *
             * Fluid is a macOS application that wraps website to look and behave like native desktop
             * applications.
             *
             * https://fluidapp.com/
             */
            FLUID_ICON,

            /**
             * An "image_src" icon.
             *
             * Yahoo and Facebook used this icon for previewing web content. Since then Facebook seems to use
             * OpenGraph instead. However website still define "image_src" icons.
             *
             * https://www.niallkennedy.com/blog/2009/03/enhanced-social-share.html
             */
            IMAGE_SRC,

            /**
             * An "Open Graph" image.
             *
             * "An image URL which should represent your object within the graph."
             *
             * http://ogp.me/
             */
            OPENGRAPH,

            /**
             * A "Twitter Card" image.
             *
             * "URL of image to use in the card."
             *
             * https://developer.twitter.com/en/docs/tweets/optimize-with-cards/overview/markup.html
             */
            TWITTER,

            /**
             * A "Microsoft tile" image.
             *
             * When pinning sites on Windows this image is used.
             *
             * "Specifies the background image for live tile."
             *
             * https://technet.microsoft.com/en-us/windows/dn255024(v=vs.60)
             */
            MICROSOFT_TILE
        }

        data class Size(
            val width: Int,
            val height: Int
        )
    }
}

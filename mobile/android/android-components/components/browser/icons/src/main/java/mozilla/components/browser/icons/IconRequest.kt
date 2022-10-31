/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

import androidx.annotation.ColorInt
import androidx.annotation.DimenRes
import mozilla.components.concept.engine.manifest.Size as HtmlSize

/**
 * A request to load an [Icon].
 *
 * @property url The URL of the website an icon should be loaded for.
 * @property size The preferred size of the icon that should be loaded.
 * @property resources An optional list of icon resources to load the icon from.
 * @property color The suggested dominant color of the icon.
 * @property isPrivate Whether this request for this icon came from a private session.
 * @property waitOnNetworkLoad Whether client code should wait on the resource being loaded or
 * loading can continue in background.
 */
data class IconRequest(
    val url: String,
    val size: Size = Size.DEFAULT,
    val resources: List<Resource> = emptyList(),
    @ColorInt val color: Int? = null,
    val isPrivate: Boolean = false,
    val waitOnNetworkLoad: Boolean = true,
) {

    /**
     * Supported sizes.
     *
     * We are trying to limit the supported sizes in order to optimize our caching strategy.
     */
    enum class Size(@DimenRes val dimen: Int) {
        DEFAULT(R.dimen.mozac_browser_icons_size_default),
        LAUNCHER(R.dimen.mozac_browser_icons_size_launcher),
        LAUNCHER_ADAPTIVE(R.dimen.mozac_browser_icons_size_launcher_adaptive),
    }

    /**
     * An icon resource that can be loaded.
     *
     * @param url URL the icon resource can be fetched from.
     * @param type The type of the icon.
     * @param sizes Optional list of icon sizes provided by this resource (if known).
     * @param mimeType Optional MIME type of this icon resource (if known).
     * @param maskable True if the icon represents as full-bleed icon that can be cropped to other shapes.
     */
    data class Resource(
        val url: String,
        val type: Type,
        val sizes: List<HtmlSize> = emptyList(),
        val mimeType: String? = null,
        val maskable: Boolean = false,
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
            MICROSOFT_TILE,

            /**
             * An icon found in Mozilla's "tippy top" list.
             */
            TIPPY_TOP,

            /**
             * A Web App Manifest image.
             *
             * https://developer.mozilla.org/en-US/docs/Web/Manifest/icons
             */
            MANIFEST_ICON,
        }
    }
}

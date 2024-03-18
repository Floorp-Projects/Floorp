/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.manifest

import androidx.annotation.ColorInt
import mozilla.components.concept.engine.manifest.WebAppManifest.ExternalApplicationResource.Fingerprint

/**
 * The web app manifest provides information about an application (such as its name, author, icon, and description).
 *
 * Web app manifests are part of a collection of web technologies called progressive web apps, which are websites
 * that can be installed to a device’s homescreen without an app store, along with other capabilities like working
 * offline and receiving push notifications.
 *
 * https://developer.mozilla.org/en-US/docs/Web/Manifest
 * https://www.w3.org/TR/appmanifest/
 * https://developers.google.com/web/fundamentals/web-app-manifest/
 *
 * @property name Provides a human-readable name for the site when displayed to the user. For example, among a list of
 * other applications or as a label for an icon.
 * @property shortName Provides a short human-readable name for the application. This is intended for when there is
 * insufficient space to display the full name of the web application, like device homescreens.
 * @property startUrl The URL that loads when a user launches the application (e.g. when added to home screen),
 * typically the index. Note that this has to be a relative URL, relative to the manifest url.
 * @property display Defines the developers’ preferred display mode for the website.
 * @property backgroundColor Defines the expected “background color” for the website. This value repeats what is
 * already available in the site’s CSS, but can be used by browsers to draw the background color of a shortcut when
 * the manifest is available before the stylesheet has loaded. This creates a smooth transition between launching the
 * web application and loading the site's content.
 * @property description Provides a general description of what the pinned website does.
 * @property icons Specifies a list of image files that can serve as application icons, depending on context. For
 * example, they can be used to represent the web application amongst a list of other applications, or to integrate the
 * web application with an OS's task switcher and/or system preferences.
 * @property dir Specifies the primary text direction for the name, short_name, and description members. Together with
 * the lang member, it helps the correct display of right-to-left languages.
 * @property lang Specifies the primary language for the values in the name and short_name members. This value is a
 * string containing a single language tag (e.g. en-US).
 * @property orientation Defines the default orientation for all the website's top level browsing contexts.
 * @property scope Defines the navigation scope of this website's context. This restricts what web pages can be viewed
 * while the manifest is applied. If the user navigates outside the scope, it returns to a normal web page inside a
 * browser tab/window.
 * @property themeColor Defines the default theme color for an application. This sometimes affects how the OS displays
 * the site (e.g., on Android's task switcher, the theme color surrounds the site).
 * @property relatedApplications List of native applications related to the web app.
 * @property preferRelatedApplications If true, related applications should be preferred over the web app.
 */
data class WebAppManifest(
    val name: String,
    val startUrl: String,
    val shortName: String? = null,
    val display: DisplayMode = DisplayMode.BROWSER,
    @ColorInt val backgroundColor: Int? = null,
    val description: String? = null,
    val icons: List<Icon> = emptyList(),
    val dir: TextDirection = TextDirection.AUTO,
    val lang: String? = null,
    val orientation: Orientation = Orientation.ANY,
    val scope: String? = null,
    @ColorInt val themeColor: Int? = null,
    val relatedApplications: List<ExternalApplicationResource> = emptyList(),
    val preferRelatedApplications: Boolean = false,
    val shareTarget: ShareTarget? = null,
) {
    /**
     * Defines the developers’ preferred display mode for the website.
     */
    enum class DisplayMode {
        /**
         * All of the available display area is used and no user agent chrome is shown.
         */
        FULLSCREEN,

        /**
         * The application will look and feel like a standalone application. This can include the application having a
         * different window, its own icon in the application launcher, etc. In this mode, the user agent will exclude
         * UI elements for controlling navigation, but can include other UI elements such as a status bar.
         */
        STANDALONE,

        /**
         * The application will look and feel like a standalone application, but will have a minimal set of UI elements
         * for controlling navigation. The elements will vary by browser.
         */
        MINIMAL_UI,

        /**
         * The application opens in a conventional browser tab or new window, depending on the browser and platform.
         * This is the default.
         */
        BROWSER,
    }

    /**
     * An image file that can serve as application icon.
     *
     * @property src The path to the image file. If src is a relative URL, the base URL will be the URL of the manifest.
     * @property sizes A list of image dimensions.
     * @property type A hint as to the media type of the image. The purpose of this member is to allow a user agent to
     * quickly ignore images of media types it does not support.
     * @property purpose Defines the purposes of the image, for example that the image is intended to serve some special
     * purpose in the context of the host OS (i.e., for better integration).
     */
    data class Icon(
        val src: String,
        val sizes: List<Size> = emptyList(),
        val type: String? = null,
        val purpose: Set<Purpose> = setOf(Purpose.ANY),
    ) {
        enum class Purpose {
            /**
             * A user agent can present this icon where space constraints and/or color requirements differ from those
             * of the application icon.
             */
            MONOCHROME,

            /**
             * The image is designed with icon masks and safe zone in mind, such that any part of the image that is
             * outside the safe zone can safely be ignored and masked away by the user agent.
             *
             * https://w3c.github.io/manifest/#icon-masks
             */
            MASKABLE,

            /**
             * The user agent is free to display the icon in any context (this is the default value).
             */
            ANY,
        }
    }

    /**
     * Defines the default orientation for all the website's top level browsing contexts.
     */
    enum class Orientation {
        ANY,
        NATURAL,
        LANDSCAPE,
        LANDSCAPE_PRIMARY,
        LANDSCAPE_SECONDARY,
        PORTRAIT,
        PORTRAIT_PRIMARY,
        PORTRAIT_SECONDARY,
    }

    /**
     * Specifies the primary text direction for the name, short_name, and description members. Together with the lang
     * member, it helps the correct display of right-to-left languages.
     */
    enum class TextDirection {
        /**
         * Left-to-right (LTR).
         */
        LTR,

        /**
         * Right-to-left (RTL).
         */
        RTL,

        /**
         * If the value is set to auto, the browser will use the Unicode bidirectional algorithm to make a best guess
         * about the text's direction.
         */
        AUTO,
    }

    /**
     * An external native application that is related to the web app.
     *
     * @property platform The platform the native app is associated with.
     * @property url The URL where it can be found.
     * @property id Information additional to or instead of the URL, depending on the platform.
     * @property minVersion The minimum version of an application related to this web app.
     * @property fingerprints [Fingerprint] objects used for verifying the application.
     */
    data class ExternalApplicationResource(
        val platform: String,
        val url: String? = null,
        val id: String? = null,
        val minVersion: String? = null,
        val fingerprints: List<Fingerprint> = emptyList(),
    ) {

        /**
         * Represents a set of cryptographic fingerprints used for verifying the application.
         * The syntax and semantics of [type] and [value] are platform-defined.
         */
        data class Fingerprint(
            val type: String,
            val value: String,
        )
    }

    /**
     * Used to define how the web app receives share data.
     * If present, a share target should be created so that other Android apps can share to this web app.
     *
     * @property action URL to open on share
     * @property method Method to use with [action]. Either "GET" or "POST".
     * @property encType MIME type to specify how the params are encoded.
     * @property params Specifies what query parameters correspond to share data.
     */
    data class ShareTarget(
        val action: String,
        val method: RequestMethod = RequestMethod.GET,
        val encType: EncodingType = EncodingType.URL_ENCODED,
        val params: Params = Params(),
    ) {

        /**
         * Specifies what query parameters correspond to share data.
         *
         * @property title Name of the query parameter used for the title of the data being shared.
         * @property text Name of the query parameter used for the body of the data being shared.
         * @property url Name of the query parameter used for a URL referring to a shared resource.
         * @property files Form fields used to share files.
         */
        data class Params(
            val title: String? = null,
            val text: String? = null,
            val url: String? = null,
            val files: List<Files> = emptyList(),
        )

        /**
         * Specifies a form field member used to share files.
         *
         * @property name Name of the form field.
         * @property accept Accepted MIME types or file extensions.
         */
        data class Files(
            val name: String,
            val accept: List<String>,
        )

        /**
         * Valid HTTP methods for [ShareTarget.method].
         */
        enum class RequestMethod {
            GET, POST
        }

        /**
         * Valid encoding MIME types for [ShareTarget.encType].
         */
        enum class EncodingType(val type: String) {
            URL_ENCODED("application/x-www-form-urlencoded"),
            MULTIPART("multipart/form-data"),
        }
    }
}

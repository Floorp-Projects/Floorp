/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

/**
 * Represents a browser extension based on the WebExtension API:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions
 *
 * @property id the unique ID of this extension.
 * @property url the url pointing to a resources path for locating the extension
 * within the APK file e.g. resource://android/assets/extensions/my_web_ext.
 */
data class WebExtension(
    val id: String,
    val url: String
)

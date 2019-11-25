/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

typealias WebExtensionBrowserAction = mozilla.components.concept.engine.webextension.BrowserAction

/**
 * Value type that represents the state of a web extension.
 *
 * @property id The unique identifier for this web extension.
 * @property url The url pointing to a resources path for locating the extension
 * within the APK file e.g. resource://android/assets/extensions/my_web_ext.
 * @property browserAction A list browser action that this web extension has.
 * @property browserActionPopupSession The ID of the session displaying
 * the browser action popup.
 */
data class WebExtensionState(
    val id: String,
    val url: String? = null,
    val browserAction: WebExtensionBrowserAction? = null,
    val browserActionPopupSession: String? = null
)

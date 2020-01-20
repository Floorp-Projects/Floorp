/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.webextension.WebExtensionBrowserAction
import mozilla.components.concept.engine.webextension.WebExtensionPageAction

/**
 * Value type that represents the state of a web extension.
 *
 * @property id The unique identifier for this web extension.
 * @property url The url pointing to a resources path for locating the extension
 * within the APK file e.g. resource://android/assets/extensions/my_web_ext.
 * @property enabled Whether or not this web extension is enabled, defaults to true.
 * @property browserAction The browser action state of this extension.
 * @property pageAction The page action state of this extension.
 * @property browserActionPopupSession The ID of the session displaying
 * the browser action popup.
 */
data class WebExtensionState(
    val id: String,
    val url: String? = null,
    val enabled: Boolean = true,
    val browserAction: WebExtensionBrowserAction? = null,
    val pageAction: WebExtensionPageAction? = null,
    val browserActionPopupSession: String? = null
)

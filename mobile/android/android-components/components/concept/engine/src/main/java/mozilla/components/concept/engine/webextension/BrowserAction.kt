/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import android.graphics.drawable.Drawable

/**
 * Value type that represents the state of a browser action within a [WebExtension].
 *
 * @property title The title of the browser action to be visible in the user interface.
 * @property enabled Indicates if the browser action should be enabled or disabled.
 * @property icon The image for this browser icon.
 * @property uri The url to get the HTML document, representing the internal user interface of the extension.
 * @property badgeText The browser action's badge text.
 * @property badgeTextColor The browser action's badge text color.
 * @property badgeBackgroundColor The browser action's badge background color.
 * @property onClick A callback to be executed when this browser action is clicked.
 */
data class BrowserAction(
    val title: String,
    val enabled: Boolean,
    val icon: Drawable,
    val uri: String,
    val badgeText: String,
    val badgeTextColor: Int,
    val badgeBackgroundColor: Int,
    val onClick: () -> Unit
)

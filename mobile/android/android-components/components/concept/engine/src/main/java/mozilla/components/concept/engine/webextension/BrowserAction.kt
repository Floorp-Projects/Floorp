/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import android.graphics.Bitmap

/**
 * Value type that represents the state of a browser action within a [WebExtension].
 *
 * @property title The title of the browser action to be visible in the user interface.
 * @property enabled Indicates if the browser action should be enabled or disabled.
 * @property loadIcon A suspending function returning the icon in the provided size.
 * @property badgeText The browser action's badge text.
 * @property badgeTextColor The browser action's badge text color.
 * @property badgeBackgroundColor The browser action's badge background color.
 * @property onClick A callback to be executed when this browser action is clicked.
 */
data class BrowserAction(
    val title: String?,
    val enabled: Boolean?,
    val loadIcon: (suspend (Int) -> Bitmap?)?,
    val badgeText: String?,
    val badgeTextColor: Int?,
    val badgeBackgroundColor: Int?,
    val onClick: () -> Unit
)

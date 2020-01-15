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
) {
    /**
     * Returns a copy of this [BrowserAction] with the provided overrides applied e.g. for tab-specific overrides.
     *
     * @param override the action to use for overriding properties. Note that only the provided
     * (non-null) properties of the override will be applied, all other properties will remain unchanged.
     */
    fun copyWithOverride(override: BrowserAction) =
        BrowserAction(
            title = override.title ?: title,
            enabled = override.enabled ?: enabled,
            badgeText = override.badgeText ?: badgeText,
            badgeBackgroundColor = override.badgeBackgroundColor ?: badgeBackgroundColor,
            badgeTextColor = override.badgeTextColor ?: badgeTextColor,
            loadIcon = override.loadIcon ?: loadIcon,
            onClick = override.onClick
        )
}

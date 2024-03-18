/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.webextension

import android.graphics.Bitmap

/**
 * Value type that represents the state of a browser or page action within a [WebExtension].
 *
 * @property title The title of the browser action to be visible in the user interface.
 * @property enabled Indicates if the browser action should be enabled or disabled.
 * @property loadIcon A suspending function returning the icon in the provided size.
 * @property badgeText The browser action's badge text.
 * @property badgeTextColor The browser action's badge text color.
 * @property badgeBackgroundColor The browser action's badge background color.
 * @property onClick A callback to be executed when this browser action is clicked.
 */
data class Action(
    val title: String?,
    val enabled: Boolean?,
    val loadIcon: (suspend (Int) -> Bitmap?)?,
    val badgeText: String?,
    val badgeTextColor: Int?,
    val badgeBackgroundColor: Int?,
    val onClick: () -> Unit,
) {
    /**
     * Returns a copy of this [Action] with the provided override applied e.g. for tab-specific overrides.
     * If the override is null, the original class is returned without making a new instance.
     *
     * @param override the action to use for overriding properties. Note that only the provided
     * (non-null) properties of the override will be applied, all other properties will remain
     * unchanged. An extension can send a tab-specific action and only include the properties
     * it wants to override for the tab.
     */
    fun copyWithOverride(override: Action?) = if (override != null) {
        Action(
            title = override.title ?: title,
            enabled = override.enabled ?: enabled,
            badgeText = override.badgeText ?: badgeText,
            badgeBackgroundColor = override.badgeBackgroundColor ?: badgeBackgroundColor,
            badgeTextColor = override.badgeTextColor ?: badgeTextColor,
            loadIcon = override.loadIcon ?: loadIcon,
            onClick = override.onClick,
        )
    } else {
        this
    }
}

typealias WebExtensionBrowserAction = Action
typealias WebExtensionPageAction = Action

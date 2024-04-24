/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * An [IconListItem] wrapper for menu items in a [MenuGroup] with an optional icon at the end.
 *
 * @param label The label in the menu item.
 * @param beforeIconPainter [Painter] used to display an [Icon] before the list item.
 * @param beforeIconDescription Content description of the icon.
 * @param description An optional description text below the label.
 * @param state The state of the menu item to display.
 * @param onClick Invoked when the user clicks on the item.
 * @param afterIconPainter [Painter] used to display an [IconButton] after the list item.
 * @param afterIconDescription Content description of the icon.
 */
@Composable
internal fun MenuItem(
    label: String,
    beforeIconPainter: Painter,
    beforeIconDescription: String? = null,
    description: String? = null,
    state: MenuItemState = MenuItemState.ENABLED,
    onClick: (() -> Unit)? = null,
    afterIconPainter: Painter? = null,
    afterIconDescription: String? = null,
) {
    val labelTextColor = getLabelTextColor(state = state)
    val iconTint = getIconTint(state = state)
    val enabled = state != MenuItemState.DISABLED

    IconListItem(
        label = label,
        labelTextColor = labelTextColor,
        description = description,
        enabled = enabled,
        onClick = onClick,
        beforeIconPainter = beforeIconPainter,
        beforeIconDescription = beforeIconDescription,
        beforeIconTint = iconTint,
        afterIconPainter = afterIconPainter,
        afterIconDescription = afterIconDescription,
        afterIconTint = iconTint,
    )
}

/**
 * An [IconListItem] wrapper for menu items in a [MenuGroup] with an optional text button at the end.
 *
 * @param label The label in the menu item.
 * @param beforeIconPainter [Painter] used to display an [Icon] before the list item.
 * @param beforeIconDescription Content description of the icon.
 * @param description An optional description text below the label.
 * @param state The state of the menu item to display.
 * @param onClick Invoked when the user clicks on the item.
 * @param afterButtonText The button text to be displayed after the list item.
 * @param afterButtonTextColor [Color] to apply to [afterButtonText].
 * @param onAfterButtonClick Called when the user clicks on the text button.
 */
@Composable
internal fun MenuItem(
    label: String,
    beforeIconPainter: Painter,
    beforeIconDescription: String? = null,
    description: String? = null,
    state: MenuItemState = MenuItemState.ENABLED,
    onClick: (() -> Unit)? = null,
    afterButtonText: String? = null,
    afterButtonTextColor: Color = FirefoxTheme.colors.actionPrimary,
    onAfterButtonClick: (() -> Unit)? = null,
) {
    val labelTextColor = getLabelTextColor(state = state)
    val iconTint = getIconTint(state = state)
    val enabled = state != MenuItemState.DISABLED

    IconListItem(
        label = label,
        labelTextColor = labelTextColor,
        description = description,
        enabled = enabled,
        onClick = onClick,
        beforeIconPainter = beforeIconPainter,
        beforeIconDescription = beforeIconDescription,
        beforeIconTint = iconTint,
        afterButtonText = afterButtonText,
        afterButtonTextColor = afterButtonTextColor,
        onAfterButtonClick = onAfterButtonClick,
    )
}

/**
 * Enum containing all the supported state for the menu item.
 */
enum class MenuItemState {
    /**
     * The menu item is enabled.
     */
    ENABLED,

    /**
     * The menu item is disabled and is not clickable.
     */
    DISABLED,

    /**
     * The menu item is highlighted to indicate the feature behind the menu item is active.
     */
    ACTIVE,

    /**
     * The menu item is highlighted to indicate the feature behind the menu item is destructive.
     */
    WARNING,
}

@Composable
private fun getLabelTextColor(state: MenuItemState): Color {
    return when (state) {
        MenuItemState.ACTIVE -> FirefoxTheme.colors.textAccent
        MenuItemState.WARNING -> FirefoxTheme.colors.textCritical
        else -> FirefoxTheme.colors.textPrimary
    }
}

@Composable
private fun getIconTint(state: MenuItemState): Color {
    return when (state) {
        MenuItemState.ACTIVE -> FirefoxTheme.colors.iconAccentViolet
        MenuItemState.WARNING -> FirefoxTheme.colors.iconCritical
        else -> FirefoxTheme.colors.iconSecondary
    }
}

@LightDarkPreview
@Composable
private fun MenuItemPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3)
                .padding(16.dp),
        ) {
            MenuGroup {
                for (state in MenuItemState.entries) {
                    MenuItem(
                        label = stringResource(id = R.string.browser_menu_translations),
                        beforeIconPainter = painterResource(id = R.drawable.mozac_ic_translate_24),
                        state = state,
                        onClick = {},
                    )

                    Divider(color = FirefoxTheme.colors.borderSecondary)
                }

                for (state in MenuItemState.entries) {
                    MenuItem(
                        label = stringResource(id = R.string.browser_menu_extensions),
                        beforeIconPainter = painterResource(id = R.drawable.mozac_ic_extension_24),
                        state = state,
                        onClick = {},
                        afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
                    )

                    Divider(color = FirefoxTheme.colors.borderSecondary)
                }

                for (state in MenuItemState.entries) {
                    MenuItem(
                        label = stringResource(id = R.string.library_bookmarks),
                        beforeIconPainter = painterResource(id = R.drawable.mozac_ic_bookmark_tray_fill_24),
                        state = state,
                        onClick = {},
                        afterButtonText = stringResource(id = R.string.browser_menu_edit),
                        onAfterButtonClick = {},
                    )

                    Divider(color = FirefoxTheme.colors.borderSecondary)
                }
            }
        }
    }
}

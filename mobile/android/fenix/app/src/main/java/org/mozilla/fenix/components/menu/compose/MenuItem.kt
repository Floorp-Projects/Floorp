/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.painter.Painter
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Wrapper around [IconListItem] for menu items that appear in a [MenuGroup].
 *
 * @param label The label in the list item.
 * @param description An optional description text below the label.
 * @param onClick Invoked when the user clicks on the item.
 * @param beforeIconPainter [Painter] used to display an [Icon] before the list item.
 * @param beforeIconDescription Content description of the icon.
 * @param afterIconPainter [Painter] used to display an [IconButton] after the list item.
 * @param afterIconDescription Content description of the icon.
 */
@Composable
internal fun MenuItem(
    label: String,
    description: String? = null,
    onClick: (() -> Unit)? = null,
    beforeIconPainter: Painter,
    beforeIconDescription: String? = null,
    afterIconPainter: Painter? = null,
    afterIconDescription: String? = null,
) {
    val iconTint = FirefoxTheme.colors.iconSecondary

    IconListItem(
        label = label,
        description = description,
        onClick = onClick,
        beforeIconPainter = beforeIconPainter,
        beforeIconDescription = beforeIconDescription,
        beforeIconTint = iconTint,
        afterIconPainter = afterIconPainter,
        afterIconDescription = afterIconDescription,
        afterIconTint = iconTint,
    )
}

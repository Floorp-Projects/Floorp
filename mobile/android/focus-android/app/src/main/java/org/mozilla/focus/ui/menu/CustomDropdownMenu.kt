/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.ui.menu

import androidx.compose.foundation.background
import androidx.compose.material.DropdownMenu
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import org.mozilla.focus.ui.theme.focusColors

/**
 * Menu used to offer a list of options in a drop-down manner when its parent is clicked
 *
 * @param menuItems The list of options
 * @param isExpanded True if the menu is open
 * @param onDismissClicked Invoked when the user dismiss the menu
 */
@Composable
fun CustomDropdownMenu(
    menuItems: List<MenuItem>,
    isExpanded: Boolean,
    onDismissClicked: () -> Unit,
) {
    DropdownMenu(
        expanded = isExpanded,
        onDismissRequest = onDismissClicked,
        modifier = Modifier.background(color = focusColors.menuBackground),
    ) {
        for (item in menuItems) {
            DropdownMenuItem(
                onClick = {
                    item.onClick()
                    onDismissClicked.invoke()
                },
            ) {
                Text(
                    text = item.title,
                    color = focusColors.menuText,
                )
            }
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.DropdownMenu
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Popup action dropdown menu.
 *
 * @param menuItems List of items to be displayed in the menu.
 * @param showMenu Whether or not the menu is currently displayed to the user.
 * @param onDismissRequest Invoked when user dismisses the menu or on orientation changes.
 * @param modifier Modifier to be applied to the menu.
 * @param offset Offset to be added to the position of the menu.
 */
@Composable
fun DropdownMenu(
    menuItems: List<MenuItem>,
    showMenu: Boolean,
    onDismissRequest: () -> Unit,
    modifier: Modifier = Modifier,
    offset: DpOffset = DpOffset.Zero,
) {
    DisposableEffect(LocalConfiguration.current.orientation) {
        onDispose { onDismissRequest() }
    }

    MaterialTheme(shapes = MaterialTheme.shapes.copy(medium = RoundedCornerShape(2.dp))) {
        DropdownMenu(
            expanded = showMenu && menuItems.isNotEmpty(),
            onDismissRequest = { onDismissRequest() },
            offset = offset,
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer2)
                .then(modifier),
        ) {
            for (item in menuItems) {
                DropdownMenuItem(
                    modifier = Modifier.testTag(item.testTag),
                    onClick = {
                        onDismissRequest()
                        item.onClick()
                    },
                ) {
                    Text(
                        text = item.title,
                        color = item.color ?: FirefoxTheme.colors.textPrimary,
                        maxLines = 1,
                        style = FirefoxTheme.typography.subtitle1,
                        modifier = Modifier
                            .fillMaxHeight()
                            .align(Alignment.CenterVertically),
                    )
                }
            }
        }
    }
}

/**
 * Represents a text item from the dropdown menu.
 *
 * @property title Text the item should display.
 * @property color Color used to display the text.
 * @property testTag Tag used to identify the item in automated tests.
 * @property onClick Callback to be called when the item is clicked.
 */
data class MenuItem(
    val title: String,
    val color: Color? = null,
    val testTag: String = "",
    val onClick: () -> Unit,
)

@LightDarkPreview
@Composable
private fun DropdownMenuPreview() {
    FirefoxTheme {
        DropdownMenu(
            listOf(
                MenuItem("Rename") {},
                MenuItem("Share") {},
                MenuItem("Remove", FirefoxTheme.colors.textWarning) {},
            ),
            true,
            {},
        )
    }
}

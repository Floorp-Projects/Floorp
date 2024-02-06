/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.DropdownMenu
import androidx.compose.material.DropdownMenuItem
import androidx.compose.material.Icon
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Root popup action dropdown menu.
 *
 * @param menuItems List of items to be displayed in the menu.
 * @param showMenu Whether or not the menu is currently displayed to the user.
 * @param cornerShape Shape to apply to the corners of the dropdown.
 * @param onDismissRequest Invoked when user dismisses the menu or on orientation changes.
 * @param modifier Modifier to be applied to the menu.
 * @param offset Offset to be added to the position of the menu.
 */
@Composable
private fun Menu(
    menuItems: List<MenuItem>,
    showMenu: Boolean,
    cornerShape: RoundedCornerShape,
    onDismissRequest: () -> Unit,
    modifier: Modifier = Modifier,
    offset: DpOffset = DpOffset.Zero,
) {
    DisposableEffect(LocalConfiguration.current.orientation) {
        onDispose { onDismissRequest() }
    }

    MaterialTheme(shapes = MaterialTheme.shapes.copy(medium = cornerShape)) {
        DropdownMenu(
            expanded = showMenu && menuItems.isNotEmpty(),
            onDismissRequest = onDismissRequest,
            offset = offset,
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer2)
                .then(modifier),
        ) {
            val hasCheckedItems = menuItems.any { it.isChecked }

            for (item in menuItems) {
                val checkmarkModifier = if (hasCheckedItems) {
                    Modifier.selectable(
                        selected = item.isChecked,
                        role = Role.Button,
                        onClick = {
                            onDismissRequest()
                            item.onClick()
                        },
                    )
                } else {
                    Modifier
                }

                DropdownMenuItem(
                    modifier = Modifier
                        .testTag(item.testTag)
                        .align(alignment = Alignment.CenterHorizontally)
                        .then(checkmarkModifier),
                    onClick = {
                        onDismissRequest()
                        item.onClick()
                    },
                ) {
                    if (hasCheckedItems) {
                        if (item.isChecked) {
                            Icon(
                                painter = painterResource(id = R.drawable.mozac_ic_checkmark_24),
                                modifier = Modifier
                                    .size(24.dp),
                                contentDescription = null,
                                tint = FirefoxTheme.colors.iconPrimary,
                            )
                        } else {
                            Spacer(
                                modifier = Modifier
                                    .size(24.dp),
                            )
                        }

                        Spacer(modifier = Modifier.width(12.dp))
                    }

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
 * Dropdown menu for presenting context-specific actions.
 *
 * @param menuItems List of items to be displayed in the menu.
 * @param showMenu Whether or not the menu is currently displayed to the user.
 * @param onDismissRequest Invoked when user dismisses the menu or on orientation changes.
 * @param modifier Modifier to be applied to the menu.
 * @param offset Offset to be added to the position of the menu.
 */
@Composable
fun ContextualMenu(
    menuItems: List<MenuItem>,
    showMenu: Boolean,
    onDismissRequest: () -> Unit,
    modifier: Modifier = Modifier,
    offset: DpOffset = DpOffset.Zero,
) {
    Menu(
        menuItems = menuItems,
        showMenu = showMenu,
        cornerShape = RoundedCornerShape(size = 5.dp),
        onDismissRequest = onDismissRequest,
        modifier = modifier,
        offset = offset,
    )
}

/**
 * Represents a text item from the dropdown menu.
 *
 * @property title Text the item should display.
 * @property color Color used to display the text.
 * @property isChecked Whether a checkmark should appear next to the text.
 * @property testTag Tag used to identify the item in automated tests.
 * @property onClick Callback to be called when the item is clicked.
 */
data class MenuItem(
    val title: String,
    val color: Color? = null,
    val isChecked: Boolean = false,
    val testTag: String = "",
    val onClick: () -> Unit,
)

@LightDarkPreview
@Composable
private fun ContextualMenuPreview() {
    var showMenu by remember { mutableStateOf(false) }
    FirefoxTheme {
        Box(modifier = Modifier.size(400.dp)) {
            PrimaryButton(text = "Show menu") {
                showMenu = true
            }

            ContextualMenu(
                menuItems = listOf(
                    MenuItem("Rename") {},
                    MenuItem("Share") {},
                    MenuItem("Remove") {},
                ),
                showMenu = showMenu,
                onDismissRequest = { showMenu = false },
            )
        }
    }
}

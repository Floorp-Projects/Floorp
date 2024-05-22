/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.list

import android.content.res.Configuration
import androidx.annotation.DrawableRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.selected
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Favicon
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.RadioButton
import org.mozilla.fenix.compose.button.TextButton
import org.mozilla.fenix.compose.ext.thenConditional
import org.mozilla.fenix.theme.FirefoxTheme

private val LIST_ITEM_HEIGHT = 56.dp

private val ICON_SIZE = 24.dp

/**
 * List item used to display a label with an optional description text and an optional
 * [IconButton] or [Icon] at the end.
 *
 * @param label The label in the list item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param maxLabelLines An optional maximum number of lines for the label text to span.
 * @param description An optional description text below the label.
 * @param maxDescriptionLines An optional maximum number of lines for the description text to span.
 * @param onClick Called when the user clicks on the item.
 * @param iconPainter [Painter] used to display an icon after the list item.
 * @param iconDescription Content description of the icon.
 * @param iconTint Tint applied to [iconPainter].
 * @param onIconClick Called when the user clicks on the icon. An [IconButton] will be
 * displayed if this is provided. Otherwise, an [Icon] will be displayed.
 */
@Composable
fun TextListItem(
    label: String,
    modifier: Modifier = Modifier,
    maxLabelLines: Int = 1,
    description: String? = null,
    maxDescriptionLines: Int = 1,
    onClick: (() -> Unit)? = null,
    iconPainter: Painter? = null,
    iconDescription: String? = null,
    iconTint: Color = FirefoxTheme.colors.iconPrimary,
    onIconClick: (() -> Unit)? = null,
) {
    ListItem(
        label = label,
        maxLabelLines = maxLabelLines,
        modifier = modifier,
        description = description,
        maxDescriptionLines = maxDescriptionLines,
        onClick = onClick,
    ) {
        if (iconPainter != null && onIconClick != null) {
            IconButton(
                onClick = onIconClick,
                modifier = Modifier
                    .padding(end = 16.dp)
                    .size(ICON_SIZE)
                    .clearAndSetSemantics {},
            ) {
                Icon(
                    painter = iconPainter,
                    contentDescription = iconDescription,
                    tint = iconTint,
                )
            }
        } else if (iconPainter != null) {
            Icon(
                painter = iconPainter,
                contentDescription = iconDescription,
                modifier = Modifier.padding(end = 16.dp),
                tint = iconTint,
            )
        }
    }
}

/**
 * List item used to display a label and a [Favicon] with an optional description text and
 * an optional [IconButton] at the end.
 *
 * @param label The label in the list item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param description An optional description text below the label.
 * @param faviconPainter Optional painter to use when fetching a new favicon is unnecessary.
 * @param onClick Called when the user clicks on the item.
 * @param url Website [url] for which the favicon will be shown.
 * @param iconPainter [Painter] used to display an [IconButton] after the list item.
 * @param iconDescription Content description of the icon.
 * @param onIconClick Called when the user clicks on the icon.
 */
@Composable
fun FaviconListItem(
    label: String,
    modifier: Modifier = Modifier,
    description: String? = null,
    faviconPainter: Painter? = null,
    onClick: (() -> Unit)? = null,
    url: String,
    iconPainter: Painter? = null,
    iconDescription: String? = null,
    onIconClick: (() -> Unit)? = null,
) {
    ListItem(
        label = label,
        modifier = modifier,
        description = description,
        onClick = onClick,
        beforeListAction = {
            if (faviconPainter != null) {
                Image(
                    painter = faviconPainter,
                    contentDescription = null,
                    modifier = Modifier
                        .padding(horizontal = 16.dp)
                        .size(ICON_SIZE),
                )
            } else {
                Favicon(
                    url = url,
                    size = ICON_SIZE,
                    modifier = Modifier.padding(horizontal = 16.dp),
                )
            }
        },
        afterListAction = {
            if (iconPainter != null && onIconClick != null) {
                IconButton(
                    onClick = onIconClick,
                    modifier = Modifier
                        .padding(end = 16.dp)
                        .size(ICON_SIZE),
                ) {
                    Icon(
                        painter = iconPainter,
                        contentDescription = iconDescription,
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                }
            }
        },
    )
}

/**
 * List item used to display a label and an icon at the beginning with an optional description
 * text and an optional [IconButton] or [Icon] at the end.
 *
 * @param label The label in the list item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param labelTextColor [Color] to be applied to the label.
 * @param description An optional description text below the label.
 * @param enabled Controls the enabled state of the list item. When `false`, the list item will not
 * be clickable.
 * @param onClick Called when the user clicks on the item.
 * @param beforeIconPainter [Painter] used to display an [Icon] before the list item.
 * @param beforeIconDescription Content description of the icon.
 * @param beforeIconTint Tint applied to [beforeIconPainter].
 * @param afterIconPainter [Painter] used to display an icon after the list item.
 * @param afterIconDescription Content description of the icon.
 * @param afterIconTint Tint applied to [afterIconPainter].
 * @param onAfterIconClick Called when the user clicks on the icon. An [IconButton] will be
 * displayed if this is provided. Otherwise, an [Icon] will be displayed.
 */
@Composable
fun IconListItem(
    label: String,
    modifier: Modifier = Modifier,
    labelTextColor: Color = FirefoxTheme.colors.textPrimary,
    description: String? = null,
    enabled: Boolean = true,
    onClick: (() -> Unit)? = null,
    beforeIconPainter: Painter,
    beforeIconDescription: String? = null,
    beforeIconTint: Color = FirefoxTheme.colors.iconPrimary,
    afterIconPainter: Painter? = null,
    afterIconDescription: String? = null,
    afterIconTint: Color = FirefoxTheme.colors.iconPrimary,
    onAfterIconClick: (() -> Unit)? = null,
) {
    ListItem(
        label = label,
        modifier = modifier,
        labelTextColor = labelTextColor,
        description = description,
        enabled = enabled,
        onClick = onClick,
        beforeListAction = {
            Icon(
                painter = beforeIconPainter,
                contentDescription = beforeIconDescription,
                modifier = Modifier.padding(horizontal = 16.dp),
                tint = if (enabled) beforeIconTint else FirefoxTheme.colors.iconDisabled,
            )
        },
        afterListAction = {
            val tint = if (enabled) afterIconTint else FirefoxTheme.colors.iconDisabled

            if (afterIconPainter != null && onAfterIconClick != null) {
                IconButton(
                    onClick = onAfterIconClick,
                    modifier = Modifier
                        .padding(end = 16.dp)
                        .size(ICON_SIZE),
                    enabled = enabled,
                ) {
                    Icon(
                        painter = afterIconPainter,
                        contentDescription = afterIconDescription,
                        tint = tint,
                    )
                }
            } else if (afterIconPainter != null) {
                Icon(
                    painter = afterIconPainter,
                    contentDescription = afterIconDescription,
                    modifier = Modifier.padding(end = 16.dp),
                    tint = tint,
                )
            }
        },
    )
}

/**
 * List item used to display a label and an icon at the beginning with an optional description
 * text and an optional [TextButton] at the end.
 *
 * @param label The label in the list item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param labelTextColor [Color] to be applied to the label.
 * @param description An optional description text below the label.
 * @param enabled Controls the enabled state of the list item. When `false`, the list item will not
 * be clickable.
 * @param onClick Called when the user clicks on the item.
 * @param beforeIconPainter [Painter] used to display an [Icon] before the list item.
 * @param beforeIconDescription Content description of the icon.
 * @param beforeIconTint Tint applied to [beforeIconPainter].
 * @param afterButtonText The button text to be displayed after the list item.
 * @param afterButtonTextColor [Color] to apply to [afterButtonText].
 * @param onAfterButtonClick Called when the user clicks on the text button.
 */
@Composable
fun IconListItem(
    label: String,
    modifier: Modifier = Modifier,
    labelTextColor: Color = FirefoxTheme.colors.textPrimary,
    description: String? = null,
    enabled: Boolean = true,
    onClick: (() -> Unit)? = null,
    beforeIconPainter: Painter,
    beforeIconDescription: String? = null,
    beforeIconTint: Color = FirefoxTheme.colors.iconPrimary,
    afterButtonText: String? = null,
    afterButtonTextColor: Color = FirefoxTheme.colors.actionPrimary,
    onAfterButtonClick: (() -> Unit)? = null,
) {
    ListItem(
        label = label,
        modifier = modifier,
        labelTextColor = labelTextColor,
        description = description,
        enabled = enabled,
        onClick = onClick,
        beforeListAction = {
            Icon(
                painter = beforeIconPainter,
                contentDescription = beforeIconDescription,
                modifier = Modifier.padding(horizontal = 16.dp),
                tint = if (enabled) beforeIconTint else FirefoxTheme.colors.iconDisabled,
            )
        },
        afterListAction = {
            if (afterButtonText != null && onAfterButtonClick != null) {
                TextButton(
                    text = afterButtonText,
                    onClick = onAfterButtonClick,
                    enabled = enabled,
                    textColor = afterButtonTextColor,
                    upperCaseText = false,
                )
            }
        },
    )
}

/**
 * List item used to display a label with an optional description text and
 * a [RadioButton] at the beginning.
 *
 * @param label The label in the list item.
 * @param selected [Boolean] That indicates whether the [RadioButton] is currently selected.
 * @param modifier [Modifier] to be applied to the layout.
 * @param maxLabelLines An optional maximum number of lines for the label text to span.
 * @param description An optional description text below the label.
 * @param maxDescriptionLines An optional maximum number of lines for the description text to span.
 * @param onClick Called when the user clicks on the item.
 */
@Composable
fun RadioButtonListItem(
    label: String,
    selected: Boolean,
    modifier: Modifier = Modifier,
    maxLabelLines: Int = 1,
    description: String? = null,
    maxDescriptionLines: Int = 1,
    onClick: (() -> Unit),
) {
    ListItem(
        label = label,
        modifier = modifier
            .clearAndSetSemantics {
                this.selected = selected
                role = Role.RadioButton
                contentDescription = if (description != null) {
                    "$label.$description"
                } else {
                    label
                }
            },
        maxLabelLines = maxLabelLines,
        description = description,
        maxDescriptionLines = maxDescriptionLines,
        onClick = onClick,
        beforeListAction = {
            RadioButton(
                selected = selected,
                modifier = Modifier
                    .padding(horizontal = 16.dp)
                    .size(ICON_SIZE)
                    .clearAndSetSemantics {},
                onClick = onClick,
            )
        },
    )
}

/**
 * List item used to display a selectable item with an icon, label description and an action
 * composable at the end.
 *
 * @param label The label in the list item.
 * @param description The description text below the label.
 * @param icon The icon to be displayed at the beginning of the list item.
 * @param isSelected The selected state of the item.
 * @param afterListAction Composable for adding UI to the end of the list item.
 * @param modifier [Modifier] to be applied to the composable.
 */
@Composable
fun SelectableListItem(
    label: String,
    description: String,
    @DrawableRes icon: Int,
    isSelected: Boolean,
    afterListAction: @Composable RowScope.() -> Unit,
    modifier: Modifier = Modifier,
) {
    ListItem(
        label = label,
        description = description,
        modifier = modifier.padding(vertical = 4.dp),
        beforeListAction = {
            SelectableItemIcon(
                icon = icon,
                isSelected = isSelected,
                modifier = Modifier.padding(start = 8.dp),
            )
        },
        afterListAction = afterListAction,
    )
}

/**
 * Icon composable that displays a checkmark icon when the item is selected.
 *
 * @param icon The icon to be displayed.
 * @param isSelected The selected state of the item.
 * @param modifier [Modifier] to be applied to the composable.
 */
@Composable
private fun SelectableItemIcon(
    @DrawableRes icon: Int,
    isSelected: Boolean,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier = modifier.size(48.dp),
        contentAlignment = Alignment.Center,
    ) {
        if (isSelected) {
            Box(
                modifier = Modifier
                    .background(
                        color = FirefoxTheme.colors.layerAccent,
                        shape = CircleShape,
                    )
                    .size(40.dp),
                contentAlignment = Alignment.Center,
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_checkmark_24),
                    contentDescription = null,
                    tint = PhotonColors.White,
                )
            }
        } else {
            Icon(
                painter = painterResource(id = icon),
                contentDescription = null,
                tint = FirefoxTheme.colors.iconPrimary,
                modifier = Modifier.size(32.dp),
            )
        }
    }
}

/**
 * Base list item used to display a label with an optional description text and
 * the flexibility to add custom UI to either end of the item.
 *
 * @param label The label in the list item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param labelTextColor [Color] to be applied to the label.
 * @param maxLabelLines An optional maximum number of lines for the label text to span.
 * @param description An optional description text below the label.
 * @param maxDescriptionLines An optional maximum number of lines for the description text to span.
 * @param enabled Controls the enabled state of the list item. When `false`, the list item will not
 * be clickable.
 * @param onClick Called when the user clicks on the item.
 * @param beforeListAction Optional Composable for adding UI before the list item.
 * @param afterListAction Optional Composable for adding UI to the end of the list item.
 */
@Composable
private fun ListItem(
    label: String,
    modifier: Modifier = Modifier,
    labelTextColor: Color = FirefoxTheme.colors.textPrimary,
    maxLabelLines: Int = 1,
    description: String? = null,
    maxDescriptionLines: Int = 1,
    enabled: Boolean = true,
    onClick: (() -> Unit)? = null,
    beforeListAction: @Composable RowScope.() -> Unit = {},
    afterListAction: @Composable RowScope.() -> Unit = {},
) {
    Row(
        modifier = modifier
            .defaultMinSize(minHeight = LIST_ITEM_HEIGHT)
            .thenConditional(
                modifier = Modifier.clickable { onClick?.invoke() },
                predicate = { onClick != null },
            ),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        beforeListAction()

        Column(
            modifier = Modifier
                .padding(horizontal = 16.dp, vertical = 6.dp)
                .weight(1f),
        ) {
            Text(
                text = label,
                color = if (enabled) labelTextColor else FirefoxTheme.colors.textDisabled,
                style = FirefoxTheme.typography.subtitle1,
                maxLines = maxLabelLines,
            )

            description?.let {
                Text(
                    text = description,
                    color = if (enabled) FirefoxTheme.colors.textSecondary else FirefoxTheme.colors.textDisabled,
                    style = FirefoxTheme.typography.body2,
                    maxLines = maxDescriptionLines,
                )
            }
        }

        afterListAction()
    }
}

@Composable
@Preview(name = "TextListItem", uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun TextListItemPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            TextListItem(label = "Label only")
        }
    }
}

@Composable
@Preview(name = "TextListItem with a description", uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun TextListItemWithDescriptionPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            TextListItem(
                label = "Label + description",
                description = "Description text",
            )
        }
    }
}

@Composable
@Preview(name = "TextListItem with a right icon", uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun TextListItemWithIconPreview() {
    FirefoxTheme {
        Column(Modifier.background(FirefoxTheme.colors.layer1)) {
            TextListItem(
                label = "Label + right icon button",
                onClick = {},
                iconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                iconDescription = "click me",
                onIconClick = { println("icon click") },
            )

            TextListItem(
                label = "Label + right icon",
                onClick = {},
                iconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                iconDescription = "click me",
            )
        }
    }
}

@Composable
@Preview(name = "IconListItem", uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun IconListItemPreview() {
    FirefoxTheme {
        Column(Modifier.background(FirefoxTheme.colors.layer1)) {
            IconListItem(
                label = "Left icon list item",
                onClick = {},
                beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                beforeIconDescription = "click me",
            )

            IconListItem(
                label = "Left icon list item",
                labelTextColor = FirefoxTheme.colors.textAccent,
                onClick = {},
                beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                beforeIconDescription = "click me",
                beforeIconTint = FirefoxTheme.colors.iconAccentViolet,
            )

            IconListItem(
                label = "Left icon list item + right icon",
                onClick = {},
                beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                beforeIconDescription = "click me",
                afterIconPainter = painterResource(R.drawable.mozac_ic_chevron_right_24),
                afterIconDescription = null,
            )

            IconListItem(
                label = "Left icon list item + right icon (disabled)",
                enabled = false,
                onClick = {},
                beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                beforeIconDescription = "click me",
                afterIconPainter = painterResource(R.drawable.mozac_ic_chevron_right_24),
                afterIconDescription = null,
            )
        }
    }
}

@Composable
@Preview(
    name = "IconListItem with after list action",
    uiMode = Configuration.UI_MODE_NIGHT_YES,
)
private fun IconListItemWithAfterListActionPreview() {
    FirefoxTheme {
        Column(Modifier.background(FirefoxTheme.colors.layer1)) {
            IconListItem(
                label = "IconListItem + right icon + clicks",
                beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                beforeIconDescription = null,
                afterIconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                afterIconDescription = "click me",
                onAfterIconClick = { println("icon click") },
            )

            IconListItem(
                label = "IconListItem + text button",
                onClick = { println("list item click") },
                beforeIconPainter = painterResource(R.drawable.mozac_ic_folder_24),
                beforeIconDescription = "click me",
                afterButtonText = "Edit",
                onAfterButtonClick = { println("text button click") },
            )
        }
    }
}

@Composable
@Preview(
    name = "FaviconListItem with a right icon and onClicks",
    uiMode = Configuration.UI_MODE_NIGHT_YES,
)
private fun FaviconListItemPreview() {
    FirefoxTheme {
        Column(Modifier.background(FirefoxTheme.colors.layer1)) {
            FaviconListItem(
                label = "Favicon + right icon + clicks",
                description = "Description text",
                onClick = { println("list item click") },
                url = "",
                iconPainter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                onIconClick = { println("icon click") },
            )

            FaviconListItem(
                label = "Favicon + painter",
                description = "Description text",
                faviconPainter = painterResource(id = R.drawable.mozac_ic_collection_24),
                onClick = { println("list item click") },
                url = "",
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun RadioButtonListItemPreview() {
    val radioOptions =
        listOf("Radio button first item", "Radio button second item", "Radio button third item")
    val (selectedOption, onOptionSelected) = remember { mutableStateOf(radioOptions[1]) }
    FirefoxTheme {
        Column(Modifier.background(FirefoxTheme.colors.layer1)) {
            radioOptions.forEach { text ->
                RadioButtonListItem(
                    label = text,
                    description = "$text description",
                    onClick = { onOptionSelected(text) },
                    selected = (text == selectedOption),
                )
            }
        }
    }
}

@Composable
@LightDarkPreview
private fun SelectableListItemPreview() {
    FirefoxTheme {
        Column(Modifier.background(FirefoxTheme.colors.layer1)) {
            SelectableListItem(
                label = "Selected item",
                description = "Description text",
                icon = R.drawable.mozac_ic_folder_24,
                isSelected = true,
                afterListAction = {},
            )

            SelectableListItem(
                label = "Non selectable item",
                description = "without after action",
                icon = R.drawable.mozac_ic_folder_24,
                isSelected = false,
                afterListAction = {},
            )

            SelectableListItem(
                label = "Non selectable item",
                description = "with after action",
                icon = R.drawable.mozac_ic_folder_24,
                isSelected = false,
                afterListAction = {
                    IconButton(onClick = {}) {
                        Icon(
                            painter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                            tint = FirefoxTheme.colors.iconPrimary,
                            contentDescription = null,
                        )
                    }
                },
            )
        }
    }
}

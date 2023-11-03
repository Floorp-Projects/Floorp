/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.list

import android.content.res.Configuration
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
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Favicon
import org.mozilla.fenix.theme.FirefoxTheme

private val LIST_ITEM_HEIGHT = 56.dp

private val ICON_SIZE = 24.dp

/**
 * List item used to display a label with an optional description text and
 * an optional [IconButton] at the end.
 *
 * @param label The label in the list item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param maxLabelLines An optional maximum number of lines for the label text to span.
 * @param description An optional description text below the label.
 * @param maxDescriptionLines An optional maximum number of lines for the description text to span.
 * @param onClick Called when the user clicks on the item.
 * @param iconPainter [Painter] used to display an [IconButton] after the list item.
 * @param iconDescription Content description of the icon.
 * @param onIconClick Called when the user clicks on the icon.
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
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }
    }
}

/**
 * List item used to display a label and a [Favicon] with an optional description text and
 * an optional [IconButton] at the end.
 *
 * @param label The label in the list item.
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
 * text and an optional [IconButton] at the end.
 *
 * @param label The label in the list item.
 * @param description An optional description text below the label.
 * @param onClick Called when the user clicks on the item.
 * @param beforeIconPainter [Painter] used to display an [Icon] before the list item.
 * @param beforeIconDescription Content description of the icon.
 * @param afterIconPainter [Painter] used to display an [IconButton] after the list item.
 * @param afterIconDescription Content description of the icon.
 * @param onAfterIconClick Called when the user clicks on the icon.
 */
@Composable
fun IconListItem(
    label: String,
    description: String? = null,
    onClick: (() -> Unit)? = null,
    beforeIconPainter: Painter,
    beforeIconDescription: String? = null,
    afterIconPainter: Painter? = null,
    afterIconDescription: String? = null,
    onAfterIconClick: (() -> Unit)? = null,
) {
    ListItem(
        label = label,
        description = description,
        onClick = onClick,
        beforeListAction = {
            Icon(
                painter = beforeIconPainter,
                contentDescription = beforeIconDescription,
                modifier = Modifier.padding(horizontal = 16.dp),
                tint = FirefoxTheme.colors.iconPrimary,
            )
        },
        afterListAction = {
            if (afterIconPainter != null && onAfterIconClick != null) {
                IconButton(
                    onClick = onAfterIconClick,
                    modifier = Modifier
                        .padding(end = 16.dp)
                        .size(ICON_SIZE),
                ) {
                    Icon(
                        painter = afterIconPainter,
                        contentDescription = afterIconDescription,
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                }
            }
        },
    )
}

/**
 * Base list item used to display a label with an optional description text and
 * the flexibility to add custom UI to either end of the item.
 *
 * @param label The label in the list item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param maxLabelLines An optional maximum number of lines for the label text to span.
 * @param description An optional description text below the label.
 * @param maxDescriptionLines An optional maximum number of lines for the description text to span.
 * @param onClick Called when the user clicks on the item.
 * @param beforeListAction Optional Composable for adding UI before the list item.
 * @param afterListAction Optional Composable for adding UI to the end of the list item.
 */
@Composable
private fun ListItem(
    label: String,
    modifier: Modifier = Modifier,
    maxLabelLines: Int = 1,
    description: String? = null,
    maxDescriptionLines: Int = 1,
    onClick: (() -> Unit)? = null,
    beforeListAction: @Composable RowScope.() -> Unit = {},
    afterListAction: @Composable RowScope.() -> Unit = {},
) {
    Row(
        modifier = when (onClick != null) {
            true -> Modifier.clickable { onClick() }
            false -> Modifier
        }.then(
            Modifier.defaultMinSize(minHeight = LIST_ITEM_HEIGHT),
        ),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        beforeListAction()

        Column(
            modifier = modifier
                .padding(horizontal = 16.dp, vertical = 6.dp)
                .weight(1f),
        ) {
            Text(
                text = label,
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.subtitle1,
                maxLines = maxLabelLines,
            )

            description?.let {
                Text(
                    text = description,
                    color = FirefoxTheme.colors.textSecondary,
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
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            TextListItem(
                label = "Label + right icon",
                iconPainter = painterResource(R.drawable.ic_menu),
                iconDescription = "click me",
                onIconClick = { println("icon click") },
            )
        }
    }
}

@Composable
@Preview(name = "IconListItem", uiMode = Configuration.UI_MODE_NIGHT_YES)
private fun IconListItemPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            IconListItem(
                label = "Left icon list item",
                beforeIconPainter = painterResource(R.drawable.ic_folder_icon),
                beforeIconDescription = "click me",
            )
        }
    }
}

@Composable
@Preview(
    name = "IconListItem with an interactable right icon",
    uiMode = Configuration.UI_MODE_NIGHT_YES,
)
private fun IconListItemWithRightIconPreview() {
    FirefoxTheme {
        Box(Modifier.background(FirefoxTheme.colors.layer1)) {
            IconListItem(
                label = "Left icon list item + right icon",
                beforeIconPainter = painterResource(R.drawable.ic_folder_icon),
                beforeIconDescription = null,
                afterIconPainter = painterResource(R.drawable.ic_menu),
                afterIconDescription = "click me",
                onAfterIconClick = { println("icon click") },
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
                iconPainter = painterResource(R.drawable.ic_menu),
                onIconClick = { println("icon click") },
            )

            FaviconListItem(
                label = "Favicon + painter",
                description = "Description text",
                faviconPainter = painterResource(id = R.drawable.ic_tab_collection),
                onClick = { println("list item click") },
                url = "",
            )
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.home

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.wallpapers.Wallpaper

/**
 * Homepage header.
 *
 * @param headerText The header string.
 * @param description The content description for the "Show all" button.
 * @param onShowAllClick Invoked when "Show all" button is clicked.
 */
@Composable
fun HomeSectionHeader(
    headerText: String,
    description: String = "",
    onShowAllClick: (() -> Unit)? = null,
) {
    if (inComposePreview) {
        HomeSectionHeaderContent(
            headerText = headerText,
            description = description,
            onShowAllClick = onShowAllClick,
        )
    } else {
        val wallpaperState = components.appStore
            .observeAsComposableState { state -> state.wallpaperState }.value

        val wallpaperAdaptedTextColor = wallpaperState?.currentWallpaper?.textColor?.let { Color(it) }

        val isWallpaperDefault =
            (wallpaperState?.currentWallpaper ?: Wallpaper.Default) == Wallpaper.Default

        HomeSectionHeaderContent(
            headerText = headerText,
            textColor = wallpaperAdaptedTextColor ?: FirefoxTheme.colors.textPrimary,
            description = description,
            showAllTextColor = if (isWallpaperDefault) {
                FirefoxTheme.colors.textAccent
            } else {
                wallpaperAdaptedTextColor ?: FirefoxTheme.colors.textAccent
            },
            onShowAllClick = onShowAllClick,
        )
    }
}

/**
 * Homepage header content.
 *
 * @param headerText The header string.
 * @param textColor [Color] to apply to the text.
 * @param description The content description for the "Show all" button.
 * @param showAllTextColor [Color] for the "Show all" button.
 * @param onShowAllClick Invoked when "Show all" button is clicked.
 */
@Composable
private fun HomeSectionHeaderContent(
    headerText: String,
    textColor: Color = FirefoxTheme.colors.textPrimary,
    description: String = "",
    showAllTextColor: Color = FirefoxTheme.colors.textAccent,
    onShowAllClick: (() -> Unit)? = null,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = headerText,
            modifier = Modifier
                .weight(1f)
                .wrapContentHeight(align = Alignment.Top),
            color = textColor,
            overflow = TextOverflow.Ellipsis,
            maxLines = 2,
            style = FirefoxTheme.typography.headline6,
        )

        onShowAllClick?.let {
            TextButton(onClick = { onShowAllClick() }) {
                Text(
                    text = stringResource(id = R.string.recent_tabs_show_all),
                    modifier = Modifier
                        .padding(start = 16.dp)
                        .semantics {
                            contentDescription = description
                        },
                    style = TextStyle(
                        color = showAllTextColor,
                        fontSize = 14.sp,
                    ),
                )
            }
        }
    }
}

@Composable
@Preview
private fun HomeSectionsHeaderPreview() {
    FirefoxTheme {
        HomeSectionHeader(
            headerText = stringResource(R.string.recently_saved_title),
            description = stringResource(R.string.recently_saved_show_all_content_description_2),
            onShowAllClick = {},
        )
    }
}

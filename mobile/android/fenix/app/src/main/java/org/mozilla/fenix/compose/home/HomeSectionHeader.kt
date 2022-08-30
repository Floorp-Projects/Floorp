/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.home

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.text.ClickableText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.SectionHeader
import org.mozilla.fenix.compose.inComposePreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme
import org.mozilla.fenix.wallpapers.Wallpaper

/**
 * Homepage header.
 *
 * @param headerText The header string.
 * @param description The description for click action
 * @param onShowAllClick Invoked when "Show all" button is clicked.
 */
@Composable
fun HomeSectionHeader(
    headerText: String,
    description: String = "",
    onShowAllClick: (() -> Unit)? = null
) {
    if (inComposePreview) {
        HomeSectionHeaderContent(
            headerText = headerText,
            description = description,
            onShowAllClick = onShowAllClick
        )
    } else {
        val wallpaperState = components.appStore
            .observeAsComposableState { state -> state.wallpaperState }.value

        val isWallpaperDefault =
            (wallpaperState?.currentWallpaper ?: Wallpaper.Default) == Wallpaper.Default

        HomeSectionHeaderContent(
            headerText = headerText,
            description = description,
            showAllTextColor = if (isWallpaperDefault) {
                FirefoxTheme.colors.textAccent
            } else {
                FirefoxTheme.colors.textPrimary
            },
            onShowAllClick = onShowAllClick,
        )
    }
}

/**
 * Homepage header content.
 *
 * @param headerText The header string.
 * @param description The description for click action
 * @param showAllTextColor [Color] for the "Show all" button.
 * @param onShowAllClick Invoked when "Show all" button is clicked.
 */
@Composable
private fun HomeSectionHeaderContent(
    headerText: String,
    description: String = "",
    showAllTextColor: Color = FirefoxTheme.colors.textAccent,
    onShowAllClick: (() -> Unit)? = null,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
    ) {
        SectionHeader(
            text = headerText,
            modifier = Modifier
                .weight(1f)
                .wrapContentHeight(align = Alignment.Top)
        )

        onShowAllClick?.let {
            ClickableText(
                text = AnnotatedString(text = stringResource(id = R.string.recent_tabs_show_all)),
                modifier = Modifier.padding(start = 16.dp)
                    .semantics {
                        contentDescription = description
                    },
                style = TextStyle(
                    color = showAllTextColor,
                    fontSize = 14.sp
                ),
                onClick = { onShowAllClick() }
            )
        }
    }
}

@Composable
@Preview
private fun HomeSectionsHeaderPreview() {
    FirefoxTheme(theme = Theme.getTheme()) {
        HomeSectionHeader(
            headerText = stringResource(R.string.recently_saved_title),
            description = stringResource(R.string.recently_saved_show_all_content_description_2),
        )
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.header.SubmenuHeader
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

internal const val SAVE_MENU_ROUTE = "save_menu"

@Suppress("LongParameterList")
@Composable
internal fun SaveSubmenu(
    isBookmarked: Boolean,
    onBackButtonClick: () -> Unit,
    onBookmarkPageMenuClick: () -> Unit,
    onEditBookmarkButtonClick: () -> Unit,
    onAddToShortcutsMenuClick: () -> Unit,
    onAddToHomeScreenMenuClick: () -> Unit,
    onSaveToCollectionMenuClick: () -> Unit,
    onSaveAsPDFMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            SubmenuHeader(
                header = stringResource(id = R.string.browser_menu_save),
                onClick = onBackButtonClick,
            )
        },
    ) {
        MenuGroup {
            BookmarkMenuItem(
                isBookmarked = isBookmarked,
                onBookmarkPageMenuClick = onBookmarkPageMenuClick,
                onEditBookmarkButtonClick = onEditBookmarkButtonClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_add_to_shortcuts),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_pin_24),
                onClick = onAddToShortcutsMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_add_to_homescreen_2),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_add_to_homescreen_24),
                onClick = onAddToHomeScreenMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_save_to_collection),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_collection_24),
                onClick = onSaveToCollectionMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_save_as_pdf),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_save_file_24),
                onClick = onSaveAsPDFMenuClick,
            )
        }
    }
}

@Composable
private fun BookmarkMenuItem(
    isBookmarked: Boolean,
    onBookmarkPageMenuClick: () -> Unit,
    onEditBookmarkButtonClick: () -> Unit,
) {
    if (isBookmarked) {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_edit_bookmark),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_bookmark_fill_24),
            state = MenuItemState.ACTIVE,
            onClick = onEditBookmarkButtonClick,
        )
    } else {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_bookmark_this_page),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_bookmark_24),
            onClick = onBookmarkPageMenuClick,
        )
    }
}

@LightDarkPreview
@Composable
private fun SaveSubmenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            SaveSubmenu(
                isBookmarked = false,
                onBackButtonClick = {},
                onBookmarkPageMenuClick = {},
                onEditBookmarkButtonClick = {},
                onAddToShortcutsMenuClick = {},
                onAddToHomeScreenMenuClick = {},
                onSaveToCollectionMenuClick = {},
                onSaveAsPDFMenuClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun SaveSubmenuPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            SaveSubmenu(
                isBookmarked = false,
                onBackButtonClick = {},
                onBookmarkPageMenuClick = {},
                onEditBookmarkButtonClick = {},
                onAddToShortcutsMenuClick = {},
                onAddToHomeScreenMenuClick = {},
                onSaveToCollectionMenuClick = {},
                onSaveAsPDFMenuClick = {},
            )
        }
    }
}

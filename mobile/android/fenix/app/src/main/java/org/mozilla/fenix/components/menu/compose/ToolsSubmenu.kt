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

internal const val TOOLS_MENU_ROUTE = "tools_menu"

@Suppress("LongParameterList")
@Composable
internal fun ToolsSubmenu(
    isReaderViewActive: Boolean,
    isTranslated: Boolean,
    onBackButtonClick: () -> Unit,
    onReaderViewMenuClick: () -> Unit,
    onTranslatePageMenuClick: () -> Unit,
    onPrintMenuClick: () -> Unit,
    onShareMenuClick: () -> Unit,
    onOpenInAppMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            SubmenuHeader(
                header = stringResource(id = R.string.browser_menu_tools),
                onClick = onBackButtonClick,
            )
        },
    ) {
        MenuGroup {
            ReaderViewMenuItem(
                isReaderViewActive = isReaderViewActive,
                onClick = onReaderViewMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            TranslationMenuItem(
                isTranslated = isTranslated,
                onClick = onTranslatePageMenuClick,
            )
        }

        MenuGroup {
            MenuItem(
                label = stringResource(id = R.string.browser_menu_print),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_print_24),
                onClick = onPrintMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_share_2),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_share_android_24),
                onClick = onShareMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_open_app_link),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_more_grid_24),
                onClick = onOpenInAppMenuClick,
            )
        }
    }
}

@Composable
private fun ReaderViewMenuItem(
    isReaderViewActive: Boolean,
    onClick: () -> Unit,
) {
    if (isReaderViewActive) {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_turn_off_reader_view),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_reader_view_fill_24),
            state = MenuItemState.ACTIVE,
            onClick = onClick,
        )
    } else {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_turn_on_reader_view),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_reader_view_24),
            onClick = onClick,
        )
    }
}

@Composable
private fun TranslationMenuItem(
    isTranslated: Boolean,
    onClick: () -> Unit,
) {
    if (isTranslated) {
        MenuItem(
            label = stringResource(
                id = R.string.browser_menu_translated_to,
                stringResource(id = R.string.app_name),
            ),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_translate_24),
            state = MenuItemState.ACTIVE,
            onClick = onClick,
        )
    } else {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_translate_page),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_translate_24),
            onClick = onClick,
        )
    }
}

@LightDarkPreview
@Composable
private fun ToolsSubmenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            ToolsSubmenu(
                isReaderViewActive = false,
                isTranslated = false,
                onBackButtonClick = {},
                onReaderViewMenuClick = {},
                onTranslatePageMenuClick = {},
                onPrintMenuClick = {},
                onShareMenuClick = {},
                onOpenInAppMenuClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun ToolsSubmenuPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            ToolsSubmenu(
                isReaderViewActive = false,
                isTranslated = false,
                onBackButtonClick = {},
                onReaderViewMenuClick = {},
                onTranslatePageMenuClick = {},
                onPrintMenuClick = {},
                onShareMenuClick = {},
                onOpenInAppMenuClick = {},
            )
        }
    }
}

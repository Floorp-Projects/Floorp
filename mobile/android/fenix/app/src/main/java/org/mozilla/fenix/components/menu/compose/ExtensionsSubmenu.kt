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
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.header.SubmenuHeader
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

internal const val EXTENSIONS_MENU_ROUTE = "extensions_menu"

@Composable
internal fun ExtensionsSubmenu(
    onBackButtonClick: () -> Unit,
    onManageExtensionsMenuClick: () -> Unit,
    onDiscoverMoreExtensionsMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            SubmenuHeader(
                header = stringResource(id = R.string.browser_menu_extensions),
                onClick = onBackButtonClick,
            )
        },
    ) {
        MenuGroup {
            MenuItem(
                label = stringResource(id = R.string.browser_menu_manage_extensions),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_extension_cog_24),
                onClick = onManageExtensionsMenuClick,
            )
        }

        MenuGroup {
            TextListItem(
                label = stringResource(id = R.string.browser_menu_discover_more_extensions),
                onClick = onDiscoverMoreExtensionsMenuClick,
                iconPainter = painterResource(R.drawable.mozac_ic_external_link_24),
                iconTint = FirefoxTheme.colors.iconSecondary,
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun ExtensionsSubmenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            ExtensionsSubmenu(
                onBackButtonClick = {},
                onManageExtensionsMenuClick = {},
                onDiscoverMoreExtensionsMenuClick = {},
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun ExtensionsSubmenuPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            ExtensionsSubmenu(
                onBackButtonClick = {},
                onManageExtensionsMenuClick = {},
                onDiscoverMoreExtensionsMenuClick = {},
            )
        }
    }
}

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
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

internal const val CUSTOM_TAB_MENU_ROUTE = "custom_tab_menu"

/**
 * Wrapper column containing the main menu items.
 *
 * @param onSwitchToDesktopSiteMenuClick Invoked when the user clicks on the switch to desktop site
 * menu toggle.
 * @param onFindInPageMenuClick Invoked when the user clicks on the find in page menu item.
 * @param onOpenInFirefoxMenuClick Invoked when the user clicks on the open in browser menu item.
 */
@Suppress("LongParameterList")
@Composable
internal fun CustomTabMenu(
    onSwitchToDesktopSiteMenuClick: () -> Unit,
    onFindInPageMenuClick: () -> Unit,
    onOpenInFirefoxMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {},
    ) {
        MenuGroup {
            MenuItem(
                label = stringResource(id = R.string.browser_menu_switch_to_desktop_site),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_device_desktop_24),
                onClick = onSwitchToDesktopSiteMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_find_in_page_2),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_search_24),
                onClick = onFindInPageMenuClick,
            )
        }

        MenuGroup {
            MenuItem(
                label = stringResource(
                    id = R.string.browser_menu_open_in_fenix,
                    stringResource(id = R.string.app_name),
                ),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_open_in),
                onClick = onOpenInFirefoxMenuClick,
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun CustomTabMenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            CustomTabMenu(
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onOpenInFirefoxMenuClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun CustomTabMenuPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            CustomTabMenu(
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onOpenInFirefoxMenuClick = {},
            )
        }
    }
}

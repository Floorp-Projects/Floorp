/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.service.fxa.manager.AccountState
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import mozilla.components.service.fxa.store.Account
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.menu.compose.header.MenuHeader
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * The menu bottom sheet dialog.
 *
 * @param accessPoint The [MenuAccessPoint] that was used to navigate to the menu dialog.
 * @param account [Account] information available for a synced account.
 * @param accountState The [AccountState] of a Mozilla account.
 * @param onMozillaAccountButtonClick Invoked when the user clicks on Mozilla account button.
 * @param onHelpButtonClick Invoked when the user clicks on the help button.
 * @param onSettingsButtonClick Invoked when the user clicks on the settings button.
 * @param onSwitchToDesktopSiteMenuClick Invoked when the user clicks on the switch to desktop site
 * menu toggle.
 * @param onFindInPageMenuClick Invoked when the user clicks on the find in page menu item.
 * @param onToolsMenuClick Invoked when the user clicks on the tools menu item.
 * @param onSaveMenuClick Invoked when the user clicks on the save menu item.
 * @param onExtensionsMenuClick Invoked when the user clicks on the extensions menu item.
 * @param onBookmarksMenuClick Invoked when the user clicks on the bookmarks menu item.
 * @param onHistoryMenuClick Invoked when the user clicks on the history menu item.
 * @param onDownloadsMenuClick Invoked when the user clicks on the downloads menu item.
 * @param onPasswordsMenuClick Invoked when the user clicks on the passwords menu item.
 * @param onCustomizeHomepageMenuClick Invoked when the user clicks on the customize
 * homepage menu item.
 * @param onNewInFirefoxMenuClick Invoked when the user clicks on the release note menu item.
 */
@Suppress("LongParameterList")
@Composable
fun MenuDialog(
    accessPoint: MenuAccessPoint,
    account: Account?,
    accountState: AccountState,
    onMozillaAccountButtonClick: () -> Unit,
    onHelpButtonClick: () -> Unit,
    onSettingsButtonClick: () -> Unit,
    onSwitchToDesktopSiteMenuClick: () -> Unit,
    onFindInPageMenuClick: () -> Unit,
    onToolsMenuClick: () -> Unit,
    onSaveMenuClick: () -> Unit,
    onExtensionsMenuClick: () -> Unit,
    onBookmarksMenuClick: () -> Unit,
    onHistoryMenuClick: () -> Unit,
    onDownloadsMenuClick: () -> Unit,
    onPasswordsMenuClick: () -> Unit,
    onCustomizeHomepageMenuClick: () -> Unit,
    onNewInFirefoxMenuClick: () -> Unit,
) {
    Column {
        MenuHeader(
            account = account,
            accountState = accountState,
            onMozillaAccountButtonClick = onMozillaAccountButtonClick,
            onHelpButtonClick = onHelpButtonClick,
            onSettingsButtonClick = onSettingsButtonClick,
        )

        Spacer(modifier = Modifier.height(8.dp))

        MainMenu(
            accessPoint = accessPoint,
            onSwitchToDesktopSiteMenuClick = onSwitchToDesktopSiteMenuClick,
            onFindInPageMenuClick = onFindInPageMenuClick,
            onToolsMenuClick = onToolsMenuClick,
            onSaveMenuClick = onSaveMenuClick,
            onExtensionsMenuClick = onExtensionsMenuClick,
            onBookmarksMenuClick = onBookmarksMenuClick,
            onHistoryMenuClick = onHistoryMenuClick,
            onDownloadsMenuClick = onDownloadsMenuClick,
            onPasswordsMenuClick = onPasswordsMenuClick,
            onCustomizeHomepageMenuClick = onCustomizeHomepageMenuClick,
            onNewInFirefoxMenuClick = onNewInFirefoxMenuClick,
        )
    }
}

/**
 * Wrapper column containing the main menu items.
 */
@Suppress("LongParameterList")
@Composable
private fun MainMenu(
    accessPoint: MenuAccessPoint,
    onSwitchToDesktopSiteMenuClick: () -> Unit,
    onFindInPageMenuClick: () -> Unit,
    onToolsMenuClick: () -> Unit,
    onSaveMenuClick: () -> Unit,
    onExtensionsMenuClick: () -> Unit,
    onBookmarksMenuClick: () -> Unit,
    onHistoryMenuClick: () -> Unit,
    onDownloadsMenuClick: () -> Unit,
    onPasswordsMenuClick: () -> Unit,
    onCustomizeHomepageMenuClick: () -> Unit,
    onNewInFirefoxMenuClick: () -> Unit,
) {
    Column(
        modifier = Modifier
            .padding(
                start = 16.dp,
                top = 12.dp,
                end = 16.dp,
                bottom = 32.dp,
            ),
        verticalArrangement = Arrangement.spacedBy(32.dp),
    ) {
        MenuGroup {
            MenuItem(
                label = stringResource(id = R.string.library_new_tab),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_plus_24),
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_new_private_tab),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_private_mode_circle_fill_24),
            )
        }

        ToolsAndActionsMenuGroup(
            accessPoint = accessPoint,
            onSwitchToDesktopSiteMenuClick = onSwitchToDesktopSiteMenuClick,
            onFindInPageMenuClick = onFindInPageMenuClick,
            onToolsMenuClick = onToolsMenuClick,
            onSaveMenuClick = onSaveMenuClick,
            onExtensionsMenuClick = onExtensionsMenuClick,
        )

        LibraryMenuGroup(
            onBookmarksMenuClick = onBookmarksMenuClick,
            onHistoryMenuClick = onHistoryMenuClick,
            onDownloadsMenuClick = onDownloadsMenuClick,
            onPasswordsMenuClick = onPasswordsMenuClick,
        )

        if (accessPoint == MenuAccessPoint.Home) {
            HomepageMenuGroup(
                onCustomizeHomepageMenuClick = onCustomizeHomepageMenuClick,
                onNewInFirefoxMenuClick = onNewInFirefoxMenuClick,
            )
        }
    }
}

@Composable
private fun ToolsAndActionsMenuGroup(
    accessPoint: MenuAccessPoint,
    onSwitchToDesktopSiteMenuClick: () -> Unit,
    onFindInPageMenuClick: () -> Unit,
    onToolsMenuClick: () -> Unit,
    onSaveMenuClick: () -> Unit,
    onExtensionsMenuClick: () -> Unit,
) {
    MenuGroup {
        if (accessPoint == MenuAccessPoint.Browser) {
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

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_tools),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_tool_24),
                onClick = onToolsMenuClick,
                afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_save),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_save_24),
                onClick = onSaveMenuClick,
                afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)
        }

        MenuItem(
            label = stringResource(id = R.string.browser_menu_extensions),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_extension_24),
            onClick = onExtensionsMenuClick,
            afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
        )
    }
}

@Composable
private fun LibraryMenuGroup(
    onBookmarksMenuClick: () -> Unit,
    onHistoryMenuClick: () -> Unit,
    onDownloadsMenuClick: () -> Unit,
    onPasswordsMenuClick: () -> Unit,
) {
    MenuGroup {
        MenuItem(
            label = stringResource(id = R.string.library_bookmarks),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_bookmark_tray_fill_24),
            onClick = onBookmarksMenuClick,
        )

        Divider(color = FirefoxTheme.colors.borderSecondary)

        MenuItem(
            label = stringResource(id = R.string.library_history),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_history_24),
            onClick = onHistoryMenuClick,
        )

        Divider(color = FirefoxTheme.colors.borderSecondary)

        MenuItem(
            label = stringResource(id = R.string.library_downloads),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_download_24),
            onClick = onDownloadsMenuClick,
        )

        Divider(color = FirefoxTheme.colors.borderSecondary)

        MenuItem(
            label = stringResource(id = R.string.browser_menu_passwords),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_login_24),
            onClick = onPasswordsMenuClick,
        )
    }
}

@Composable
private fun HomepageMenuGroup(
    onCustomizeHomepageMenuClick: () -> Unit,
    onNewInFirefoxMenuClick: () -> Unit,
) {
    MenuGroup {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_customize_home_1),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_grid_add_24),
            onClick = onCustomizeHomepageMenuClick,
        )

        Divider(color = FirefoxTheme.colors.borderSecondary)

        MenuItem(
            label = stringResource(
                id = R.string.browser_menu_new_in_firefox,
                stringResource(id = R.string.app_name),
            ),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_whats_new_24),
            onClick = onNewInFirefoxMenuClick,
        )
    }
}

@LightDarkPreview
@Composable
private fun MenuDialogPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            MenuDialog(
                accessPoint = MenuAccessPoint.Home,
                account = null,
                accountState = NotAuthenticated,
                onMozillaAccountButtonClick = {},
                onHelpButtonClick = {},
                onSettingsButtonClick = {},
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onToolsMenuClick = {},
                onSaveMenuClick = {},
                onExtensionsMenuClick = {},
                onBookmarksMenuClick = {},
                onHistoryMenuClick = {},
                onDownloadsMenuClick = {},
                onPasswordsMenuClick = {},
                onCustomizeHomepageMenuClick = {},
                onNewInFirefoxMenuClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun MenuDialogPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            MenuDialog(
                accessPoint = MenuAccessPoint.Home,
                account = null,
                accountState = NotAuthenticated,
                onMozillaAccountButtonClick = {},
                onHelpButtonClick = {},
                onSettingsButtonClick = {},
                onSwitchToDesktopSiteMenuClick = {},
                onFindInPageMenuClick = {},
                onToolsMenuClick = {},
                onSaveMenuClick = {},
                onExtensionsMenuClick = {},
                onBookmarksMenuClick = {},
                onHistoryMenuClick = {},
                onDownloadsMenuClick = {},
                onPasswordsMenuClick = {},
                onCustomizeHomepageMenuClick = {},
                onNewInFirefoxMenuClick = {},
            )
        }
    }
}

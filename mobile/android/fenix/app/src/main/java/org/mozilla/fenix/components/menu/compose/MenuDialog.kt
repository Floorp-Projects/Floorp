/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * The menu bottom sheet dialog.
 */
@Composable
fun MenuDialog() {
    Column {
        MainMenu()
    }
}

/**
 * Wrapper column containing the main menu items.
 */
@Composable
private fun MainMenu() {
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
            IconListItem(
                label = stringResource(id = R.string.library_new_tab),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_plus_24),
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            IconListItem(
                label = stringResource(id = R.string.browser_menu_new_private_tab),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_private_mode_circle_fill_24),
            )
        }
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
            MenuDialog()
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
            MenuDialog()
        }
    }
}

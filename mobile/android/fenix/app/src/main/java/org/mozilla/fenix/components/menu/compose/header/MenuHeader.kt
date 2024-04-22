/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose.header

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.service.fxa.manager.AccountState
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import mozilla.components.service.fxa.store.Account
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

@Composable
internal fun MenuHeader(
    account: Account?,
    accountState: AccountState,
    onMozillaAccountButtonClick: () -> Unit,
    onHelpButtonClick: () -> Unit,
    onSettingsButtonClick: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 12.dp, end = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        MozillaAccountMenuButton(
            account = account,
            accountState = accountState,
            onClick = onMozillaAccountButtonClick,
            modifier = Modifier.weight(1f),
        )

        Spacer(modifier = Modifier.width(8.dp))

        Divider(modifier = Modifier.size(width = 2.dp, height = 32.dp))

        IconButton(
            onClick = onHelpButtonClick,
        ) {
            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_help_circle_24),
                contentDescription = null,
                tint = FirefoxTheme.colors.iconSecondary,
            )
        }

        IconButton(
            onClick = onSettingsButtonClick,
        ) {
            Icon(
                painter = painterResource(id = R.drawable.mozac_ic_settings_24),
                contentDescription = null,
                tint = FirefoxTheme.colors.iconSecondary,
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun MenuHeaderPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            MenuHeader(
                account = null,
                accountState = NotAuthenticated,
                onMozillaAccountButtonClick = {},
                onHelpButtonClick = {},
                onSettingsButtonClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun MenuHeaderPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            MenuHeader(
                account = null,
                accountState = NotAuthenticated,
                onMozillaAccountButtonClick = {},
                onHelpButtonClick = {},
                onSettingsButtonClick = {},
            )
        }
    }
}

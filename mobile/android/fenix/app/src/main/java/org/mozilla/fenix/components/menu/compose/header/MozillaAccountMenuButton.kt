/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose.header

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.service.fxa.manager.AccountState
import mozilla.components.service.fxa.manager.AccountState.Authenticated
import mozilla.components.service.fxa.manager.AccountState.Authenticating
import mozilla.components.service.fxa.manager.AccountState.AuthenticationProblem
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import mozilla.components.service.fxa.store.Account
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Image
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

private val BUTTON_HEIGHT = 56.dp
private val BUTTON_SHAPE = RoundedCornerShape(size = 8.dp)
private val ICON_SHAPE = RoundedCornerShape(size = 24.dp)
private val AVATAR_SIZE = 24.dp

@Composable
internal fun MozillaAccountMenuButton(
    account: Account?,
    accountState: AccountState,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
            .background(
                color = FirefoxTheme.colors.layer3,
                shape = BUTTON_SHAPE,
            )
            .clip(shape = BUTTON_SHAPE)
            .clickable { onClick() }
            .defaultMinSize(minHeight = BUTTON_HEIGHT),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Spacer(modifier = Modifier.width(4.dp))

        AvatarIcon(account)

        Column(
            modifier = Modifier
                .padding(horizontal = 8.dp)
                .weight(1f),
        ) {
            when (accountState) {
                NotAuthenticated -> {
                    Text(
                        text = stringResource(id = R.string.browser_menu_sign_in),
                        color = FirefoxTheme.colors.textSecondary,
                        maxLines = 1,
                        style = FirefoxTheme.typography.headline7,
                    )

                    Text(
                        text = stringResource(id = R.string.browser_menu_sign_in_caption),
                        color = FirefoxTheme.colors.textSecondary,
                        maxLines = 2,
                        style = FirefoxTheme.typography.caption,
                    )
                }

                AuthenticationProblem -> {
                    Text(
                        text = stringResource(id = R.string.browser_menu_sign_back_in_to_sync),
                        color = FirefoxTheme.colors.textSecondary,
                        maxLines = 1,
                        style = FirefoxTheme.typography.headline7,
                    )

                    Text(
                        text = stringResource(id = R.string.browser_menu_syncing_paused_caption),
                        color = FirefoxTheme.colors.textCritical,
                        maxLines = 2,
                        style = FirefoxTheme.typography.caption,
                    )
                }

                Authenticated -> {
                    Text(
                        text = account?.displayName ?: account?.email
                            ?: stringResource(id = R.string.browser_menu_account_settings),
                        color = FirefoxTheme.colors.textSecondary,
                        maxLines = 1,
                        style = FirefoxTheme.typography.headline7,
                    )
                }

                is Authenticating -> Unit
            }
        }

        if (accountState == AuthenticationProblem) {
            Icon(
                painter = painterResource(R.drawable.mozac_ic_warning_fill_24),
                contentDescription = null,
                tint = FirefoxTheme.colors.iconCritical,
            )

            Spacer(modifier = Modifier.width(8.dp))
        }
    }
}

@Composable
private fun FallbackAvatarIcon() {
    Icon(
        painter = painterResource(id = R.drawable.mozac_ic_avatar_circle_24),
        contentDescription = null,
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer2,
                shape = ICON_SHAPE,
            )
            .padding(all = 4.dp),
        tint = FirefoxTheme.colors.iconSecondary,
    )
}

@Composable
private fun AvatarIcon(account: Account?) {
    val avatarUrl = account?.avatar?.url

    if (avatarUrl != null) {
        Image(
            url = avatarUrl,
            modifier = Modifier
                .background(
                    color = FirefoxTheme.colors.layer2,
                    shape = ICON_SHAPE,
                )
                .padding(all = 4.dp)
                .size(AVATAR_SIZE)
                .clip(CircleShape),
            targetSize = AVATAR_SIZE,
            placeholder = { FallbackAvatarIcon() },
            fallback = { FallbackAvatarIcon() },
        )
    } else {
        FallbackAvatarIcon()
    }
}

@Composable
private fun MenuHeaderPreviewContent() {
    Column(
        modifier = Modifier
            .background(color = FirefoxTheme.colors.layer2)
            .padding(all = 16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        MozillaAccountMenuButton(
            account = null,
            accountState = NotAuthenticated,
            onClick = {},
        )

        MozillaAccountMenuButton(
            account = null,
            accountState = AuthenticationProblem,
            onClick = {},
        )

        MozillaAccountMenuButton(
            account = Account(
                uid = "testUID",
                avatar = null,
                email = "test@example.com",
                displayName = "test profile",
                currentDeviceId = null,
                sessionToken = null,
            ),
            accountState = Authenticated,
            onClick = {},
        )

        MozillaAccountMenuButton(
            account = Account(
                uid = "testUID",
                avatar = null,
                email = "test@example.com",
                displayName = null,
                currentDeviceId = null,
                sessionToken = null,
            ),
            accountState = Authenticated,
            onClick = {},
        )

        MozillaAccountMenuButton(
            account = Account(
                uid = "testUID",
                avatar = null,
                email = null,
                displayName = null,
                currentDeviceId = null,
                sessionToken = null,
            ),
            accountState = Authenticated,
            onClick = {},
        )
    }
}

@LightDarkPreview
@Composable
private fun MenuHeaderPreview() {
    FirefoxTheme {
        MenuHeaderPreviewContent()
    }
}

@Preview
@Composable
private fun MenuHeaderPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        MenuHeaderPreviewContent()
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.identitycredential

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.concept.identitycredential.Account
import mozilla.components.concept.identitycredential.Provider
import mozilla.components.feature.prompts.R
import mozilla.components.feature.prompts.identitycredential.previews.DialogPreviewMaterialTheme
import mozilla.components.feature.prompts.identitycredential.previews.LightDarkPreview
import mozilla.components.support.ktx.kotlin.base64ToBitmap

/**
 * A Federated Credential Management dialog for selecting an account.
 *
 * @param provider The [Provider] on which the user is logging in.
 * @param colors The colors of the dialog.
 * @param accounts The list of available accounts for this provider.
 * @param modifier [Modifier] to be applied to the layout.
 * @param onAccountClick Invoked when the user clicks on an item.
 */
@Composable
fun SelectAccountDialog(
    provider: Provider,
    accounts: List<Account>,
    modifier: Modifier = Modifier,
    colors: DialogColors = DialogColors.default(),
    onAccountClick: (Account) -> Unit,
) {
    Column(
        modifier = modifier.fillMaxWidth(),
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.padding(16.dp),
        ) {
            provider.icon?.base64ToBitmap()?.asImageBitmap()?.let {
                Image(
                    bitmap = it,
                    contentDescription = null,
                    contentScale = ContentScale.FillWidth,
                    modifier = Modifier
                        .size(16.dp),
                )

                Spacer(Modifier.width(4.dp))
            }

            Text(
                text = stringResource(
                    id = R.string.mozac_feature_prompts_identity_credentials_choose_account_for_provider,
                    provider.name,
                ),
                style = TextStyle(
                    fontSize = 16.sp,
                    lineHeight = 24.sp,
                    color = colors.title,
                    letterSpacing = 0.15.sp,
                ),
            )
        }

        accounts.forEach { account ->
            AccountItem(account = account, colors = colors, onClick = onAccountClick)
        }

        Spacer(modifier = Modifier.height(16.dp))
    }
}

@Composable
private fun AccountItem(
    account: Account,
    modifier: Modifier = Modifier,
    colors: DialogColors = DialogColors.default(),
    onClick: (Account) -> Unit,
) {
    IdentityCredentialItem(
        title = account.name,
        description = account.email,
        colors = colors,
        modifier = modifier,
        onClick = { onClick(account) },
    ) {
        account.icon?.base64ToBitmap()?.asImageBitmap()?.let { bitmap ->
            Image(
                bitmap = bitmap,
                contentDescription = null,
                contentScale = ContentScale.FillWidth,
                modifier = Modifier
                    .padding(horizontal = 16.dp)
                    .size(32.dp),
            )
        } ?: Spacer(
            Modifier
                .padding(horizontal = 16.dp)
                .width(32.dp),
        )
    }
}

@Composable
@Preview(name = "Provider with no favicon")
private fun AccountItemPreview() {
    AccountItem(
        modifier = Modifier.background(Color.White),
        account = Account(
            0,
            "user@mozilla.com",
            "User",
            USER_PICTURE,
        ),
        onClick = {},
    )
}

@Composable
@LightDarkPreview
private fun SelectAccountDialogPreview() {
    DialogPreviewMaterialTheme {
        SelectAccountDialog(
            provider = Provider(0, GOOGLE_FAVICON, "Google", "google.com"),
            accounts = listOf(
                Account(
                    0,
                    "user@mozilla.com",
                    "User",
                    USER_PICTURE,
                ),
                Account(
                    1,
                    "user2@mozilla.com",
                    "Google",
                    null,
                ),
            ),
            modifier = Modifier.background(Color.White),
            onAccountClick = { },
        )
    }
}

@Suppress("MaxLineLength")
private const val GOOGLE_FAVICON =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAARCAYAAADUryzEAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAH+SURBVHgBhVO/b9NQEL5z7Bi1lWJUCYRYHBaWDh6SKhspAYnRoVO3SqxIbiUGEEIRQ1kbxB9AM7VISJiNoSUeI6DCYmSpt1ZUak2U0GLXuZ7dOnUsp73l+e593/343hkhZYePKqp/EhhAoLOrRkEEmwgd/mrd3PpmJvGYdPbvl1cJYQkuMQI085KwfP1Lxwl9Mb74Uyu/J4BFuMIEoKp/HCixHyXYf1BqEF2QuS13gPQ2P1OyQt//ta0CYgOBFApg7ob13R5ij9rX1P67uzuDv/k45ASBMHfLOmsxabvVipqOo78pNZls/Nu8Df7vAgRBrphFHmfhCPeEggdT8zvg2dNrk8/2Rsi1N12dx1OyyIjghgnUOCBrB5/TIAJhNYkZuSNwBT4vsiNlVrrEFJHf3UE6q7DtTfO5N4Jg5W3u1Um0pPBza+eeE4nIH8aHozvQ7M+4J3JQtOumO65kbaU33BfWwOK9IPN5dzYkRy1JntAYR3640tOSy8YatKJVLm3Mt/moJrAWEL7+sfDRCp3qJ13peYIxsft0SezPxjo5X19OFaMEGgOk/7mflK12OM5QXPlAB/mwDjjA+tarSXP4M1XWdXVCLrS7Xi8ryUhCsV9a7jx5sRbpkL4trz9eJBQMnlBLExncypHU7CxsOHEQx5WJ5j4WoyQiiE6SlLRTu5e/Z+DrlXsAAAAASUVORK5CYII="

@Suppress("MaxLineLength")
private const val USER_PICTURE =
    "data:image/png;base64,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"
